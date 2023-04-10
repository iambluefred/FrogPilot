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
    {"BackButton", "'Back' Button", "Replace the 'X' button in the settings menu with a cleaner looking 'Back' button.", "../assets/offroad/icon_back.png"},
    {"DisableAd", "Disable comma prime Ad", "Disables the comma prime ad.", "../assets/offroad/icon_minus.png"},
    {"DisableInternetCheck", "Disable Internet Check", "Allows the device to be offline indefinitely.", "../assets/offroad/icon_warning.png"},
    {"FireTheBabysitter", "Fire the Babysitter", "Disable some of openpilot's 'Babysitter Protocols'.", "../assets/offroad/icon_babysitter.png"},
    {"NumericalTemp", "Numerical Temperature Gauge", "Replace openpilot's 'GOOD', 'OK', and 'HIGH' temperature statuses with numerical readings.", "../assets/offroad/icon_temp.png"},
    {"Sidebar", "Sidebar Shown By Default", "Sidebar is shown by default while onroad as opposed to hidden.", "../assets/offroad/icon_metric.png"},
    {"WideCameraDisable", "Wide Camera Overridden", "Turns off the wide camera from displaying while onroad. This toggle is purely cosmetic and won't prevent openpilot from using the wide camera.", "../assets/offroad/icon_camera.png"}
  };

  for (int i = 0; i < toggles.size(); i++) {
    const auto &[key, label, desc, icon] = toggles[i];
    if (key == "FrogTheme") {
      createSubControl(key, label, desc, icon, {}, {
        {"FrogColors", "Enable FrogPilot Colors", "Replace stock openpilot colors with FrogPilot's."},
        {"FrogIcons", "Enable FrogPilot Icons", "Replace stock openpilot icons with FrogPilot's."},
        {"FrogSounds", "Enable FrogPilot Sounds", "Replace stock openpilot sounds with FrogPilot's."}
      });
    } else if (key == "FireTheBabysitter") {
      createSubControl(key, label, desc, icon, {}, {
        {"MuteDM", "Disable Driver Monitoring", "Disables the driver monitoring system."},
        {"MuteDoor", "Mute 'Door Open' alert", "Mutes the 'Door Open' alert."},
        {"MuteSeatbelt", "Mute 'Seatbelt Unlatched' alert", "Mutes the 'Seatbelt Unlatched' alert."}
      });
    } else {
      mainLayout->addWidget(createParamControl(key, label, desc, icon, parent));
      if (i != toggles.size() - 1) mainLayout->addWidget(horizontal_line());
    }
  }
}

ParamControl *FrogPilotPanel::createParamControl(const QString &key, const QString &label, const QString &desc, const QString &icon, QWidget *parent) {
  ParamControl *control = new ParamControl(key, label, desc, icon);
  connect(control, &ParamControl::toggleFlipped, [=](bool state) {
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
