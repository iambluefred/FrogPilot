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
    {"FrogTheme", "FrogPilot Theme", "Enable the beloved FrogPilot Theme! Disable toggle to revert back to the stock openpilot theme.", "../assets/images/frog_button_home.png"}
  };

  for (int i = 0; i < toggles.size(); i++) {
    const auto &[key, label, desc, icon] = toggles[i];
    if (key == "FrogTheme") {
      createSubControl(key, label, desc, icon, {}, {
        {"FrogColors", "FrogPilot Colors", "Replace stock openpilot colors with FrogPilot's."},
        {"FrogIcons", "FrogPilot Icons", "Replace stock openpilot icons with FrogPilot's."},
        {"FrogSounds", "FrogPilot Sounds", "Replace stock openpilot sounds with FrogPilot's."}
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
