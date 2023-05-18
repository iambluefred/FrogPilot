#include <QHBoxLayout>
#include <QWidget>
#include <tuple>
#include <vector>

#include "common/params.h"
#include "selfdrive/ui/qt/offroad/frogpilot_settings.h"
#include "selfdrive/ui/qt/widgets/input.h"
#include "selfdrive/ui/ui.h"

FrogPilotPanel::FrogPilotPanel(QWidget *parent) : QWidget(parent) {
  mainLayout = new QVBoxLayout(this);

  const std::vector<std::tuple<QString, QString, QString, QString>> toggles = {
    {"FrogTheme", "FrogPilot Theme", "Enable the beloved FrogPilot Theme! Disable toggle to revert back to the stock openpilot theme.", "../assets/images/frog_button_home.png"},
    {"AdjustableFollowDistance", "Adjustable Follow Distance", "Enable FrogPilot's follow distance profiles using the 'Distance' button on the steering wheel (Toyota/Lexus Only) or via the onroad UI for other makes.\n\n1 bar = Aggressive\n2 bars = Comfort\n3 bars = Relaxed", "../assets/offroad/icon_distance.png"},
    {"AlwaysOnLateral", "Always on Lateral (Toyota/Lexus Only)", "Enable 'Always On Lateral' to keep openpilot lateral control when using either the brake or gas pedals. openpilot is only disengaged by deactivating the 'Cruise Control' button.", "../assets/offroad/icon_disengage_on_accelerator.svg"},
    {"Compass", "Compass", "Compass that rotates according to your driving direction.", "../assets/offroad/icon_compass.png"},
    {"ConditionalExperimentalMode", "Conditional Experimental Mode", "Automatically activate 'Experimental Mode' based on specified conditions.", "../assets/offroad/icon_conditional.png"},
    {"CustomRoadUI", "Custom Road UI", "Customize the road UI to your liking.", "../assets/offroad/icon_road.png"},
    {"DeviceShutdownTimer", "Device Shutdown Timer", "Set the timer for when the device turns off after being offroad to reduce energy waste and prevent battery drain.", "../assets/offroad/icon_time.png"},
    {"DisableAd", "Disable comma prime Ad", "Hides the comma prime ad.", "../assets/offroad/icon_minus.png"},
    {"DisableInternetCheck", "Disable Internet Check", "Allows the device to remain offline indefinitely.", "../assets/offroad/icon_warning.png"},
    {"ExperimentalModeViaWheel", "Experimental Mode Via Steering Wheel / Screen", "Enable or disable Experimental Mode by double-clicking the 'Lane Departure'/LKAS button on the steering wheel (Toyota/Lexus Only) or double tapping the screen for other makes.\n\nOverrides 'Conditional Experimental Mode'. ", "../assets/img_experimental_white.svg"},
    {"FireTheBabysitter", "Fire the Babysitter", "Disable some of openpilot's 'Babysitter Protocols'.", "../assets/offroad/icon_babysitter.png"},
    {"NudgelessLaneChange", "Nudgeless Lane Change", "Switch lanes without having to nudge the steering wheel.", "../assets/offroad/icon_lane.png"},
    {"NumericalTemp", "Numerical Temperature Gauge", "Replace openpilot's 'GOOD', 'OK', and 'HIGH' temperature statuses with numerical values.", "../assets/offroad/icon_temp.png"},
    {"PathColorTesting", "Path Color Testing", "Sets the color hue for testing the path color.", "../assets/offroad/icon_blank.png"},
    {"PersonalTune", "Personal Tune", "Enable the values of my personal tune which drives a bit more aggressively.", "../assets/offroad/icon_tune.png"},
    {"RotatingWheel", "Rotating Steering Wheel", "The steering wheel in top right corner of the onroad UI rotates alongside your physical steering wheel.", "../assets/offroad/icon_rotate.png"},
    {"ScreenBrightness", "Screen Brightness", "Choose a custom screen brightness level or use the default 'Auto' brightness setting.", "../assets/offroad/icon_light.png"},
    {"Sidebar", "Sidebar Shown By Default", "Sidebar is shown by default while onroad as opposed to hidden.", "../assets/offroad/icon_metric.png"},
    {"SilentMode", "Silent Mode", "Disables all openpilot sounds for a completely silent experience.", "../assets/offroad/icon_mute.png"},
    {"SteeringWheel", "Steering Wheel Icon", "Replace the stock openpilot steering wheel icon with a custom icon.\n\nWant to submit your own steering wheel? Message me on Discord:\nFrogsGoMoo #6969.", "../assets/offroad/icon_openpilot.png"},
    {"WideCameraDisable", "Wide Camera Disabled (Cosmetic Only)", "Disable the wide camera display while onroad. This toggle is purely cosmetic and will not affect openpilot's use of the wide camera.", "../assets/offroad/icon_camera.png"}
  };

  for (int i = 0; i < toggles.size(); i++) {
    const auto &[key, label, desc, icon] = toggles[i];
    if (key == "FrogTheme") {
      createSubControl(key, label, desc, icon, {}, {
        {"FrogColors", "FrogPilot Colors", "Replace stock openpilot colors with FrogPilot's."},
        {"FrogIcons", "FrogPilot Icons", "Replace stock openpilot icons with FrogPilot's."},
        {"FrogSignals", "FrogPilot Signals", "Add a turn signal animation of a frog hopping across the screen."},
        {"FrogSounds", "FrogPilot Sounds", "Replace stock openpilot sounds with FrogPilot's."}
      });
    } else if (key == "ConditionalExperimentalMode") {
      createSubControl(key, label, desc, icon, {
        new ConditionalExperimentalModeSpeed(),
        new ConditionalExperimentalModeSpeedLead(),
      }, {
        {"ConditionalExperimentalModeStopLights", "Experimental Mode For Stop Signs/Lights", "Activate 'Experimental Mode' whenever a stop sign or stop light is detected."},
        {"ConditionalExperimentalModeCurves", "Experimental Mode On Curves", "Activate 'Experimental Mode' for curves."},
        {"ConditionalExperimentalModeCurvesLead", "   Don't Activate On Curves With Lead", "Don't activate 'Experimental Mode' on curves with a lead vehicle."},
        {"ConditionalExperimentalModeSignal", "Experimental Mode With Turn Signal", "Activate 'Experimental Mode' whenever the turn signal is on to take turns."}
      });
    } else if (key == "CustomRoadUI") {
      createSubControl(key, label, desc, icon, {
        new LaneLinesWidth(),
        new PathEdgeWidth(),
        new PathWidth(),
        new RoadEdgesWidth()
      }, {
        {"UnlimitedLength", "'Unlimited' Length", "Increases the path and road lines to extend out as far as the model can see."}
      });
    } else if (key == "DeviceShutdownTimer") {
      mainLayout->addWidget(new DeviceShutdownTimer());
      mainLayout->addWidget(horizontal_line());
    } else if (key == "FireTheBabysitter") {
      createSubControl(key, label, desc, icon, {}, {
        {"MuteDM", "Disable Driver Monitoring", "Disables the driver monitoring system."},
        {"MuteDoor", "Mute 'Door Open' alert", "Mutes the 'Door Open' alert."},
        {"MuteSeatbelt", "Mute 'Seatbelt Unlatched' alert", "Mutes the 'Seatbelt Unlatched' alert."},
        {"MuteSystemOverheat", "Mute 'System Overheated' alert", "Mutes the 'System Overheated' alert."}
      });
    } else if (key == "NudgelessLaneChange") {
      createSubControl(key, label, desc, icon, {}, {
        {"LaneDetection", "Lane Detection", "Prevents automatic lane changes if no lane is detected to turn into. Helps prevent early lane changes such as preparing for an upcoming left/right turn."},
        {"OneLaneChange", "One Lane Change Per Signal", "Limits nudgeless lane changes to one per turn signal activation. Helps prevent lane changes when preparing for an upcoming left/right turn with no barrier between you and the other side of the road."},
      });
    } else if (key == "PathColorTesting") {
      mainLayout->addWidget(new PathColorTesting());
      mainLayout->addWidget(horizontal_line());
    } else if (key == "PersonalTune") {
      createSubControl(key, label, desc, icon, {}, {
        {"ExperimentalPersonalTune", "Experimental Personal Tune", "An algorithm that I have developed that dynamically adjusts the following distance when approaching a slower lead vehicle, and then gradually increases it to emulate more human-like driving behavior. Work in progress; use at your own risk."},
      });
    } else if (key == "ScreenBrightness") {
      mainLayout->addWidget(new ScreenBrightness());
      mainLayout->addWidget(horizontal_line());
    } else if (key == "SteeringWheel") {
      mainLayout->addWidget(new SteeringWheel());
      mainLayout->addWidget(horizontal_line());
    } else {
      mainLayout->addWidget(createParamControl(key, label, desc, icon, parent));
      if (i != toggles.size() - 1) mainLayout->addWidget(horizontal_line());
    }
  }
}

ParamControl *FrogPilotPanel::createParamControl(const QString &key, const QString &label, const QString &desc, const QString &icon, QWidget *parent) {
  ParamControl *control = new ParamControl(key, label, desc, icon);
  connect(control, &ParamControl::toggleFlipped, [=](bool state) {
    if (key == "PersonalTune") {
      if (Params().getBool("PersonalTune")) {
        ConfirmationDialog::toggleAlert("WARNING: This will reduce the following distance, increase acceleration, and modify openpilot's braking behavior!", "I understand the risks.", parent);
      }
    }
    if (key == "ExperimentalPersonalTune") {
      if (Params().getBool("ExperimentalPersonalTune")) {
        ConfirmationDialog::toggleAlert("WARNING: This is EXTREMELY experimental and can cause the car to drive dangerously!", "I understand the risks.", parent);
      }
    }
    if (ConfirmationDialog::toggle("Reboot required to take effect.", "Reboot Now", parent)) {
      Hardware::reboot();
    }
  });
  return control;
}

void FrogPilotPanel::addControl(const QString &key, const QString &label, const QString &desc, QVBoxLayout *layout, const QString &icon, bool addHorizontalLine) {
  layout->addWidget(createParamControl(key, label, desc, icon, this));
  if (addHorizontalLine) layout->addWidget(horizontal_line());
}

QWidget *FrogPilotPanel::addSubControls(const QString &parentKey, QVBoxLayout *layout, const std::vector<std::tuple<QString, QString, QString>> &controls) {
  QWidget *mainControl = new QWidget(this);
  mainControl->setLayout(layout);
  mainLayout->addWidget(mainControl);
  mainControl->setVisible(Params().getBool(parentKey.toStdString()));
  for (const auto &[key, label, desc] : controls) addControl(key, "   " + label, desc, layout);
  return mainControl;
}

void FrogPilotPanel::createSubControl(const QString &key, const QString &label, const QString &desc, const QString &icon, const std::vector<QWidget*> &subControls, const std::vector<std::tuple<QString, QString, QString>> &additionalControls) {
  ParamControl *control = createParamControl(key, label, desc, icon, this);
  mainLayout->addWidget(control);
  mainLayout->addWidget(horizontal_line());
  QVBoxLayout *subControlLayout = new QVBoxLayout();
  for (QWidget *subControl : subControls) {
    subControlLayout->addWidget(subControl);
    subControlLayout->addWidget(horizontal_line());
  }
  QWidget *mainControl = addSubControls(key, subControlLayout, additionalControls);
  connect(control, &ParamControl::toggleFlipped, [=](bool state) { mainControl->setVisible(state); });
}
