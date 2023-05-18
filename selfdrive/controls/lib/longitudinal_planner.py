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
from selfdrive.controls.lib.drive_helpers import V_CRUISE_MAX, CONTROL_N, get_speed_error
from system.swaglog import cloudlog

LON_MPC_STEP = 0.2  # first step is 0.2s
A_CRUISE_MIN = -1.2
A_CRUISE_MIN_VALS_PERSONAL_TUNE = [-0.6, -0.7, -0.8, -0.6]
A_CRUISE_MIN_BP_PERSONAL_TUNE = [0., 10.0, 25., 40.]
A_CRUISE_MAX_VALS = [1.6, 1.2, 0.8, 0.6]
A_CRUISE_MAX_VALS_PERSONAL_TUNE = [3.5, 3.0, 1.5, 1.0]
A_CRUISE_MAX_BP = [0., 10.0, 25., 40.]

# Lookup table for turns
_A_TOTAL_MAX_V = [1.7, 3.2]
_A_TOTAL_MAX_BP = [20., 40.]

# Time threshold for Conditional Experimental Mode (Code runs at 20hz, so: THRESHOLD / 20 = seconds)
THRESHOLD = 5 # 0.25s

# Lookup table for stop sign / stop light detection. Credit goes to the DragonPilot team!
STOP_SIGN_BREAKING_POINT = [0., 10., 20., 30., 40., 50., 55.]
STOP_SIGN_DISTANCE = [10, 30., 50., 70., 80., 90., 120.]

def get_min_accel_personal_tune(v_ego):
  return interp(v_ego, A_CRUISE_MIN_BP_PERSONAL_TUNE, A_CRUISE_MIN_VALS_PERSONAL_TUNE)

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

    # FrogPilot variables
    self.personal_tune = self.CP.personalTune
    # Set variables for Conditional Experimental Mode
    self.params = Params()
    self.conditional_experimental_mode = self.CP.conditionalExperimentalMode
    self.experimental_mode_via_wheel = self.CP.experimentalModeViaWheel
    self.curves = self.params.get_bool("ConditionalExperimentalModeCurves")
    self.curves_lead = self.params.get_bool("ConditionalExperimentalModeCurvesLead")
    self.limit = self.params.get_int("ConditionalExperimentalModeSpeed") * CV.MPH_TO_MS
    self.limit_lead = self.params.get_int("ConditionalExperimentalModeSpeedLead") * CV.MPH_TO_MS
    self.signal = self.params.get_bool("ConditionalExperimentalModeSignal")
    self.stop_lights = self.params.get_bool("ConditionalExperimentalModeStopLights")
    self.curve = False
    self.limits_checked = True
    self.new_experimental_mode = False
    self.previous_lead_status = False
    self.curvature_count = 0
    self.lead_status_count = 0
    self.previous_status_value = 0
    self.stop_light_count = 0

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
    v_ego_kph = v_ego * 3.6
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
        accel_limits = [get_min_accel_personal_tune(v_ego), get_max_accel_personal_tune(v_ego)]
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
    self.v_model_error = get_speed_error(sm['modelV2'], v_ego)

    if force_slow_decel:
      v_cruise = 0.0
    # clip limits, cannot init MPC outside of bounds
    accel_limits_turns[0] = min(accel_limits_turns[0], self.a_desired + 0.05)
    accel_limits_turns[1] = max(accel_limits_turns[1], self.a_desired - 0.05)

    self.mpc.set_accel_limits(accel_limits_turns[0], accel_limits_turns[1])
    self.mpc.set_cur_state(self.v_desired_filter.x, self.a_desired)
    x, v, a, j = self.parse_model(sm['modelV2'], self.v_model_error)
    self.mpc.update(sm['carState'], sm['radarState'], v_cruise, x, v, a, j, prev_accel_constraint, self.personal_tune)

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
        
    # Lead detection
    def detect_lead(radarState):
      lead_status = radarState.leadOne.status
      self.lead_status_count = (self.lead_status_count + 1) if lead_status == self.previous_lead_status else 0
      self.previous_lead_status = lead_status
      # Check if lead is detected for > 0.25s
      has_lead = self.lead_status_count >= THRESHOLD and lead_status
      return has_lead

    # Determine the road curvature - Credit goes to to Pfeiferj!
    def road_curvature(lead, modelData, standstill):
      # Check if there's no lead vehicle if necessary
      if not (self.curves_lead and lead) and not standstill and self.curves:
        predicted_lateral_accelerations = np.abs(np.array(modelData.acceleration.y))
        predicted_velocities = np.array(modelData.velocity.x)
        # Check if the number of predicted lateral accelerations and velocities are as expected
        if len(predicted_lateral_accelerations) != 0 and len(predicted_lateral_accelerations) == len(predicted_velocities):
          curvature_ratios = predicted_lateral_accelerations / (predicted_velocities ** 2)
          predicted_lateral_accelerations = curvature_ratios * (v_ego ** 2)
          # Return max lateral accelerations
          curvature = np.amax(predicted_lateral_accelerations)
          if curvature >= 1.6 or (self.curve and curvature > 1.1):
            # Setting the minimum to 10 lets it hold the status for 0.25s after it goes "false" to help prevent false negatives
            self.curvature_count = min(10, self.curvature_count + 1)
          else:
            self.curvature_count = max(0, self.curvature_count - 1)
          # Check if curve is detected for > 0.25s
          return self.curvature_count >= THRESHOLD
      return False
      
    # Stop sign and stop light detection - Credit goes to the DragonPilot team!
    def stop_sign_and_light(carState, lead, modelData, standstill):
      if abs(carState.steeringAngleDeg) <= 60 and not standstill and self.stop_lights:
        if len(modelData.orientation.x) == len(modelData.position.x) == TRAJECTORY_SIZE:
          if modelData.position.x[TRAJECTORY_SIZE - 1] < interp(v_ego_kph, STOP_SIGN_BREAKING_POINT, STOP_SIGN_DISTANCE):
            self.stop_light_count = min(10, self.stop_light_count + 1)
          else:
            self.stop_light_count = max(0, self.stop_light_count - 1)
        else:
            self.stop_light_count = max(0, self.stop_light_count - 1)
      else:
        self.stop_light_count = max(0, self.stop_light_count - 1)
      # Check if stop sign / stop light is detected for > 0.25s
      return self.stop_light_count >= THRESHOLD

    # Conditional Experimental Mode
    def conditional_experimental_mode():
      # Retrieve the required driving conditions
      carState, modelData, radarState = (sm[key] for key in ['carState', 'modelV2', 'radarState'])
      lead = detect_lead(radarState)
      experimental_mode = self.mpc.mode == 'blended'
      standstill = carState.standstill

      # Set the value of "overridden"
      if self.experimental_mode_via_wheel:
        if carState.steeringWheelCar:
          overridden = carState.conditionalOverridden
        else:
          overridden = self.params.get_int("ConditionalStatus")
      else:
        overridden = 0

      # Only check and update the limits once
      if not self.limits_checked:
        if self.params.get_bool("FrogPilotTogglesUpdated"):
          self.limit = self.params.get_int("ConditionalExperimentalModeSpeed") * CV.MPH_TO_MS
          self.limit_lead = self.params.get_int("ConditionalExperimentalModeSpeedLead") * CV.MPH_TO_MS
        self.limits_checked = True

      # Update conditions
      self.curve = road_curvature(lead, modelData, standstill)
      # If either turn signal is on and the car is driving < 55 mph
      signal = self.signal and v_ego < 25 and (carState.leftBlinker or carState.rightBlinker)
      speed = (self.limit != 0 and not lead and v_ego < self.limit) or (self.limit_lead != 0 and lead and v_ego < self.limit_lead)
      stop_light_detected = stop_sign_and_light(carState, lead, modelData, standstill)
      conditions_met = self.curve or signal or speed or stop_light_detected

      # Update Experimental Mode based on the current driving conditions
      if (not experimental_mode and conditions_met and overridden != 1) or overridden == 2:
        self.new_experimental_mode = True
      elif (experimental_mode and not conditions_met and not standstill and overridden != 2) or overridden == 1:
        # Check the speed limits just in case the user changed either of their values mid drive
        self.limits_checked = False
        self.new_experimental_mode = False

      # Set parameter for on-road status bar
      if (overridden in [1, 2]) and not carState.steeringWheelCar:
        status_value = overridden
      else:
        status_value = 3 if signal else 4 if stop_light_detected else 5 if self.curve else 6 if speed and not lead else 7 if speed else 0
      # Update status value if it has changed
      if status_value != self.previous_status_value:
        self.params.put_int("ConditionalStatus", status_value)
        self.previous_status_value = status_value

    # Only run the function if Conditional Experimental Mode is on and cruise control is activated
    if self.conditional_experimental_mode and sm['carState'].cruiseState.available:
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

    longitudinalPlan.conditionalExperimentalMode = self.new_experimental_mode

    pm.send('longitudinalPlan', plan_send)
