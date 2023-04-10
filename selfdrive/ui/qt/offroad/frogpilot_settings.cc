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
    {"100Brightness", "100% Screen Brightness", "Screen is always at 100% Brightness.", "../assets/offroad/icon_light.png"},
    {"6Hour", "6 Hour Shutdown", "Device turns off after being offroad for 6 hours instead of 30 to reduce energy waste and battery drain.", "../assets/offroad/icon_time.png"},
    {"AdjustableFollowDistance", "Adjustable Follow Distance", "Enable the ability to adjust the follow distance via the 'Distance' button on the steering wheel. 1 bar = Aggressive, 2 bar = Normal, 3 = Relaxed (Toyota/Lexus Only)", "../assets/offroad/icon_distance.png"},
    {"BackButton", "'Back' Button", "Replace the 'X' button in the settings menu with a cleaner looking 'Back' button.", "../assets/offroad/icon_back.png"},
    {"Compass", "Compass", "Add a compass in bottom right corner of the onroad that rotates according to the direction you're driving.", "../assets/offroad/icon_compass.png"},
    {"ConditionalExperimentalMode", "Conditional Experimental Mode", "Have openpilot automatically switch between 'Experimental Mode' and 'Chill Mode' depending on specified conditions.", "../assets/offroad/icon_conditional.png"},
    {"CustomRoadUI", "Custom Road UI", "Create a unique openpilot road interface that reflects your individual style.", "../assets/offroad/icon_road.png"},
    {"DisableAd", "Disable comma prime Ad", "Disables the comma prime ad.", "../assets/offroad/icon_minus.png"},
    {"DisableInternetCheck", "Disable Internet Check", "Allows the device to be offline indefinitely.", "../assets/offroad/icon_warning.png"},
    {"ExperimentalModeSteeringWheel", "Enable Experimental Mode Via Steering Wheel", "Enable Experimental Mode by 'double clicking' the 'Lane Departure'/LKAS button on the steering wheel. This will override 'Conditional Experimental Mode' (Toyota/Lexus Only)", "../assets/img_experimental_white.svg"},
    {"FireTheBabysitter", "Fire the Babysitter", "Disable some of openpilot's 'Babysitter Protocols'.", "../assets/offroad/icon_babysitter.png"},
    {"NudgelessLaneChange", "Nudgeless Lane Change", "Change lanes without the need to nudge the steering wheel first.", "../assets/offroad/icon_lane.png"},
    {"NumericalTemp", "Numerical Temperature Gauge", "Replace openpilot's 'GOOD', 'OK', and 'HIGH' temperature statuses with numerical readings.", "../assets/offroad/icon_temp.png"},
    {"PersonalTune", "Personal Tune", "Enable the values of my personal tune which drives a bit more aggressively.", "../assets/offroad/icon_tune.png"},
    {"RotatingWheel", "Rotating Steering Wheel", "The steering wheel in top right corner of the onroad UI rotates alongside your physical steering wheel.", "../assets/offroad/icon_rotate.png"},
    {"Sidebar", "Sidebar Shown By Default", "Sidebar is shown by default while onroad as opposed to hidden.", "../assets/offroad/icon_metric.png"},
    {"SteeringWheel", "Steering Wheel Icon", "Replace the stock openpilot steering wheel icon with a custom icon.", "../assets/offroad/icon_openpilot.png"},
    {"WideCameraDisable", "Wide Camera Overridden", "Turns off the wide camera from displaying while onroad. This toggle is purely cosmetic and won't prevent openpilot from using the wide camera.", "../assets/offroad/icon_camera.png"}
  };

  for (int i = 0; i < toggles.size(); i++) {
    const auto &[key, label, desc, icon] = toggles[i];
    if (key == "FrogTheme") {
      createSubControl(key, label, desc, icon, {}, {
        {"FrogColors", "Enable FrogPilot Colors", "Replace stock openpilot colors with FrogPilot's."},
        {"FrogIcons", "Enable FrogPilot Icons", "Replace stock openpilot icons with FrogPilot's."},
        {"FrogSignals", "Enable FrogPilot Signals", "Add turn signals to the onroad UI in the form of an animation of a frog hopping across the screen."},
        {"FrogSounds", "Enable FrogPilot Sounds", "Replace stock openpilot sounds with FrogPilot's."}
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
    } else if (key == "ConditionalExperimentalMode") {
      createSubControl(key, label, desc, icon, {
        new ConditionalExperimentalModeSpeed(),
      }, {
        {"ConditionalExperimentalModeCurves", "Switch to Experimental Mode On Curves", "Switch to 'Experimental Mode' when on curves."},
        {"ConditionalExperimentalModeCurvesLead", "   Don't Switch On Curves With Lead", "Don't switch to 'Experimental Mode' when on curves with a lead vehicle."},
        {"ConditionalExperimentalModeSignal", "Switch to Experimental Mode On Turn Signal", "Switch to 'Experimental Mode' whenever the turn signal is on for left and right turns."}
      });
    } else if (key == "FireTheBabysitter") {
      createSubControl(key, label, desc, icon, {}, {
        {"MuteDM", "Disable Driver Monitoring", "Disables the driver monitoring system."},
        {"MuteDoor", "Mute 'Door Open' alert", "Mutes the 'Door Open' alert."},
        {"MuteSeatbelt", "Mute 'Seatbelt Unlatched' alert", "Mutes the 'Seatbelt Unlatched' alert."}
      });
    } else if (key == "NudgelessLaneChange") {
      createSubControl(key, label, desc, icon, {}, {
        {"LaneDetection", "Lane Detection", "Prevents automatic lane changes if no lane is detected to turn into. Helps prevent early lane changes such as preparing for an upcoming left/right turn."},
        {"OneLaneChange", "One Lane Change Per Signal", "Limits nudgeless lane changes to one per turn signal activation. Helps prevent lane changes when preparing for an upcoming left/right turn with no barrier between you and the other side of the road."},
      });
    } else if (key == "PersonalTune") {
      createSubControl(key, label, desc, icon, {}, {
        {"ExperimentalPersonalTune", "Experimental Personal Tune", "Adjusts acceleration and deceleration based on speed difference and distance to the lead car. Work in progress; use at your own risk."},
      });
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
