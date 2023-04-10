#!/usr/bin/env python3
import math
import numpy as np
from common.numpy_fast import clip, interp

import cereal.messaging as messaging
from common.conversions import Conversions as CV
from common.filter_simple import FirstOrderFilter
from common.params import Params
from common.realtime import DT_MDL
from selfdrive.modeld.constants import T_IDXS
from selfdrive.controls.lib.lateral_planner import TRAJECTORY_SIZE
from selfdrive.controls.lib.longcontrol import LongCtrlState
from selfdrive.controls.lib.longitudinal_mpc_lib.long_mpc import LongitudinalMpc, MIN_ACCEL, MAX_ACCEL
from selfdrive.controls.lib.longitudinal_mpc_lib.long_mpc import T_IDXS as T_IDXS_MPC
from selfdrive.controls.lib.drive_helpers import V_CRUISE_MAX, CONTROL_N
from system.swaglog import cloudlog

LON_MPC_STEP = 0.2  # first step is 0.2s
A_CRUISE_MIN = -1.2
A_CRUISE_MAX_VALS = [1.6, 1.2, 0.8, 0.6]
A_CRUISE_MAX_VALS_PERSONAL_TUNE = [1.6, 2.0, 1.2, 1.0]
A_CRUISE_MAX_BP = [0., 10.0, 25., 40.]

# Lookup table for turns
_A_TOTAL_MAX_V = [1.7, 3.2]
_A_TOTAL_MAX_BP = [20., 40.]


def get_max_accel(v_ego):
  return interp(v_ego, A_CRUISE_MAX_BP, A_CRUISE_MAX_VALS)

def get_max_accel_personal_tune(v_ego):
  return interp(v_ego, A_CRUISE_MAX_BP, A_CRUISE_MAX_VALS_PERSONAL_TUNE)

def limit_accel_in_turns(v_ego, angle_steers, a_target, CP):
  """
  This function returns a limited long acceleration allowed, depending on the existing lateral acceleration
  this should avoid accelerating when losing the target in turns
  """

  # FIXME: This function to calculate lateral accel is incorrect and should use the VehicleModel
  # The lookup table for turns should also be updated if we do this
  a_total_max = interp(v_ego, _A_TOTAL_MAX_BP, _A_TOTAL_MAX_V)
  a_y = v_ego ** 2 * angle_steers * CV.DEG_TO_RAD / (CP.steerRatio * CP.wheelbase)
  a_x_allowed = math.sqrt(max(a_total_max ** 2 - a_y ** 2, 0.))

  return [a_target[0], min(a_target[1], a_x_allowed)]


class LongitudinalPlanner:
  def __init__(self, CP, init_v=0.0, init_a=0.0):
    self.CP = CP
    self.mpc = LongitudinalMpc(CP)
    self.fcw = False

    self.a_desired = init_a
    self.v_desired_filter = FirstOrderFilter(init_v, 2.0, DT_MDL)
    self.v_model_error = 0.0

    self.v_desired_trajectory = np.zeros(CONTROL_N)
    self.a_desired_trajectory = np.zeros(CONTROL_N)
    self.j_desired_trajectory = np.zeros(CONTROL_N)
    self.solverExecutionTime = 0.0

    self.personal_tune = self.CP.personalTune

    # Set variables for Conditional Experimental Mode
    self.params = Params()
    self.conditional_experimental = self.params.get_bool("ConditionalExperimentalMode")
    self.curves = self.conditional_experimental and self.params.get_bool("ConditionalExperimentalModeCurves")
    self.curves_lead = self.conditional_experimental and self.params.get_bool("ConditionalExperimentalModeCurvesLead")
    self.limit = float(self.params.get("ConditionalExperimentalModeSpeed")) * CV.MPH_TO_MS
    self.signal = self.conditional_experimental and self.params.get_bool("ConditionalExperimentalModeSignal")
    self.curve = False
    self.limit_checked = True
    self.override_reset = False
    self.previous_status_value = 0

  @staticmethod
  def parse_model(model_msg, model_error):
    if (len(model_msg.position.x) == 33 and
       len(model_msg.velocity.x) == 33 and
       len(model_msg.acceleration.x) == 33):
      x = np.interp(T_IDXS_MPC, T_IDXS, model_msg.position.x) - model_error * T_IDXS_MPC
      v = np.interp(T_IDXS_MPC, T_IDXS, model_msg.velocity.x) - model_error
      a = np.interp(T_IDXS_MPC, T_IDXS, model_msg.acceleration.x)
      j = np.zeros(len(T_IDXS_MPC))
    else:
      x = np.zeros(len(T_IDXS_MPC))
      v = np.zeros(len(T_IDXS_MPC))
      a = np.zeros(len(T_IDXS_MPC))
      j = np.zeros(len(T_IDXS_MPC))
    return x, v, a, j

  def update(self, sm):
    self.mpc.mode = 'blended' if sm['controlsState'].experimentalMode else 'acc'

    v_ego = sm['carState'].vEgo
    v_cruise_kph = sm['controlsState'].vCruise
    v_cruise_kph = min(v_cruise_kph, V_CRUISE_MAX)
    v_cruise = v_cruise_kph * CV.KPH_TO_MS

    long_control_off = sm['controlsState'].longControlState == LongCtrlState.off
    force_slow_decel = sm['controlsState'].forceDecel

    # Reset current state when not engaged, or user is controlling the speed
    reset_state = long_control_off if self.CP.openpilotLongitudinalControl else not sm['controlsState'].enabled

    # No change cost when user is controlling the speed, or when standstill
    prev_accel_constraint = not (reset_state or sm['carState'].standstill)

    if self.mpc.mode == 'acc':
      if self.personal_tune:
        accel_limits = [A_CRUISE_MIN, get_max_accel_personal_tune(v_ego)]
      else:
        accel_limits = [A_CRUISE_MIN, get_max_accel(v_ego)]
      accel_limits_turns = limit_accel_in_turns(v_ego, sm['carState'].steeringAngleDeg, accel_limits, self.CP)
    else:
      accel_limits = [MIN_ACCEL, MAX_ACCEL]
      accel_limits_turns = [MIN_ACCEL, MAX_ACCEL]

    if reset_state:
      self.v_desired_filter.x = v_ego
      # Clip aEgo to cruise limits to prevent large accelerations when becoming active
      self.a_desired = clip(sm['carState'].aEgo, accel_limits[0], accel_limits[1])

    # Prevent divergence, smooth in current v_ego
    self.v_desired_filter.x = max(0.0, self.v_desired_filter.update(v_ego))
    # Compute model v_ego error
    if len(sm['modelV2'].temporalPose.trans):
      self.v_model_error = sm['modelV2'].temporalPose.trans[0] - v_ego

    if force_slow_decel:
      v_cruise = 0.0
    # clip limits, cannot init MPC outside of bounds
    accel_limits_turns[0] = min(accel_limits_turns[0], self.a_desired + 0.05)
    accel_limits_turns[1] = max(accel_limits_turns[1], self.a_desired - 0.05)

    self.mpc.set_accel_limits(accel_limits_turns[0], accel_limits_turns[1])
    self.mpc.set_cur_state(self.v_desired_filter.x, self.a_desired)
    x, v, a, j = self.parse_model(sm['modelV2'], self.v_model_error)
    self.mpc.update(self.CP, sm['carState'], sm['radarState'], v_cruise, x, v, a, j, prev_accel_constraint)

    self.v_desired_trajectory_full = np.interp(T_IDXS, T_IDXS_MPC, self.mpc.v_solution)
    self.a_desired_trajectory_full = np.interp(T_IDXS, T_IDXS_MPC, self.mpc.a_solution)
    self.v_desired_trajectory = self.v_desired_trajectory_full[:CONTROL_N]
    self.a_desired_trajectory = self.a_desired_trajectory_full[:CONTROL_N]
    self.j_desired_trajectory = np.interp(T_IDXS[:CONTROL_N], T_IDXS_MPC[:-1], self.mpc.j_solution)

    # TODO counter is only needed because radar is glitchy, remove once radar is gone
    self.fcw = self.mpc.crash_cnt > 2 and not sm['carState'].standstill
    if self.fcw:
      cloudlog.info("FCW triggered")

    # Interpolate 0.05 seconds and save as starting point for next iteration
    a_prev = self.a_desired
    self.a_desired = float(interp(DT_MDL, T_IDXS[:CONTROL_N], self.a_desired_trajectory))
    self.v_desired_filter.x = self.v_desired_filter.x + DT_MDL * (self.a_desired + a_prev) / 2.0

    # Set the lead vehicle's status
    lead = sm['radarState'].leadOne.status

    # Determine the road curvature
    def road_curvature():
      # Retrieve the model data
      model_data = sm['modelV2'] if sm.valid.get('modelV2', False) else None
      # Check if curves are enabled, if there's no lead vehicle if necessary, model_data is available, and if the number of lane lines and their size are as expected
      if self.curves and not (self.curves_lead and lead) and model_data and len(model_data.laneLines) == 4 and len(model_data.laneLines[0].t) == TRAJECTORY_SIZE:
        # Extract left and right lane probabilities
        left_prob, right_prob = model_data.laneLineProbs[1], model_data.laneLineProbs[2]
        # Check if both left and right lane probabilities are above the threshold (0.6)
        if left_prob > 0.6 and right_prob > 0.6:
          # Get the x-coordinates of the left lane line
          lane_x = model_data.laneLines[1].x
          # Compute the center y-coordinates of the lane by averaging left and right lane y-coordinates
          center_y = ((np.array(model_data.laneLines[2].y) - np.array(model_data.laneLines[1].y)) / 2 + np.array(model_data.laneLines[1].y))
          # Fit a 3rd degree polynomial to the lane's x and center_y points
          path_polynomial = np.polyfit(lane_x, center_y, 3)
          # Generate an array of x_values for the curvature calculation
          x_values = np.linspace(20, 150, num=30)
          # Calculate the curvature for each x_value, and return the maximum curvature multiplied by the square of ego vehicle's speed
          return (v_ego**2 * np.amax(np.vectorize(lambda x: abs(2 * path_polynomial[1] + 6 * path_polynomial[0] * x) / (1 + (3 * path_polynomial[0] * x**2 + 2 * path_polynomial[1] * x + path_polynomial[2])**2)**(1.5))(x_values)))
      # Return 0 if the conditions for curvature calculation are not met.
      return 0

    # Conditional Experimental Mode
    def conditional_experimental_mode():
      # Retrieve the road curvature data
      curvature = road_curvature()

      # Set the current status of ExperimentalMode
      experimental_mode = self.mpc.mode == 'blended'

      # Only check and update the limit once
      if not self.limit_checked:
        self.limit = float(self.params.get("ConditionalExperimentalModeSpeed")) * CV.MPH_TO_MS
        self.limit_checked = True

      # Update conditions
      self.curve = curvature >= 1.6 or (self.curve and curvature > 1.2)
      signal = self.signal and v_ego < self.limit and (sm['carState'].leftBlinker or sm['carState'].rightBlinker)
      speed = not lead and v_ego < self.limit
      conditions_met = self.curve or speed or signal

      # Update Experimental Mode and reset the ConditionalOverridden parameter
      if not experimental_mode and conditions_met:
        if int(self.params.get("ConditionalOverridden") or 0) != 1:
          self.params.put_bool('ExperimentalMode', True)
          self.override_reset = False
      elif experimental_mode and not conditions_met and not sm['carState'].standstill:
        if int(self.params.get("ConditionalOverridden") or 0) != 2:
          self.params.put_bool('ExperimentalMode', False)
          # Check the speed limit just in case the user changed it's value mid drive
          self.limit_checked = False
          self.override_reset = False

      # Reset the ConditionalOverridden parameter
      if not self.override_reset and not conditions_met:
        self.params.put("ConditionalOverridden", "0")
        self.override_reset = True

      # Set parameter for on-road status bar
      status_value = 1 if self.curve else 2 if signal else 3 if speed else 0
      # Update status value if it has changed
      if status_value != self.previous_status_value:
        self.params.put("ConditionalStatus", str(status_value))
        self.previous_status_value = status_value

    if self.conditional_experimental:
      conditional_experimental_mode()

  def publish(self, sm, pm):
    plan_send = messaging.new_message('longitudinalPlan')

    plan_send.valid = sm.all_checks(service_list=['carState', 'controlsState'])

    longitudinalPlan = plan_send.longitudinalPlan
    longitudinalPlan.modelMonoTime = sm.logMonoTime['modelV2']
    longitudinalPlan.processingDelay = (plan_send.logMonoTime / 1e9) - sm.logMonoTime['modelV2']

    longitudinalPlan.speeds = self.v_desired_trajectory.tolist()
    longitudinalPlan.accels = self.a_desired_trajectory.tolist()
    longitudinalPlan.jerks = self.j_desired_trajectory.tolist()

    longitudinalPlan.hasLead = sm['radarState'].leadOne.status
    longitudinalPlan.longitudinalPlanSource = self.mpc.source
    longitudinalPlan.fcw = self.fcw

    longitudinalPlan.solverExecutionTime = self.mpc.solve_time

    pm.send('longitudinalPlan', plan_send)
