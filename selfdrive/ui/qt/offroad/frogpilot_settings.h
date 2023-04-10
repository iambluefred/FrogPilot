#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QMap>
#include <QTimer>

#include "selfdrive/ui/qt/widgets/controls.h"

class FrogPilotPanel : public QWidget {
  Q_OBJECT

public:
  explicit FrogPilotPanel(QWidget *parent = nullptr);

private:
  QVBoxLayout *mainLayout;

  ParamControl *createParamControl(const QString &key, const QString &label, const QString &desc, const QString &icon, QWidget *parent);
  void addControl(const QString &key, const QString &label, const QString &desc, QVBoxLayout *layout, const QString &icon = "../assets/offroad/icon_blank.png", bool addHorizontalLine = true);
  QWidget *addSubControls(const QString &parentKey, QVBoxLayout *layout, const std::vector<std::tuple<QString, QString, QString>> &controls);
  void createSubControl(const QString &key, const QString &label, const QString &desc, const QString &icon, const std::vector<QWidget*> &subControls, const std::vector<std::tuple<QString, QString, QString>> &additionalControls = {});
};

class ParamValueControl : public AbstractControl {
protected:
  ParamValueControl(const QString& name, const QString& description, const QString& iconPath) : AbstractControl(name, description, iconPath) {
    label.setAlignment(Qt::AlignVCenter | Qt::AlignRight);
    label.setStyleSheet("color: #e0e879");
    setButtonStyle(btnminus, "-");
    setButtonStyle(btnplus, "+");
    hlayout->addWidget(&label);
    hlayout->addWidget(&btnminus);
    hlayout->addWidget(&btnplus);
    connect(&btnminus, &QPushButton::pressed, [this]() { updateValue(-1); minusTimer.start(200); });
    connect(&btnminus, &QPushButton::released, [this]() { minusTimer.stop(); });
    connect(&btnplus, &QPushButton::pressed, [this]() { updateValue(1); plusTimer.start(200); });
    connect(&btnplus, &QPushButton::released, [this]() { plusTimer.stop(); });
    connect(&minusTimer, &QTimer::timeout, [this]() { updateValue(-1); });
    connect(&plusTimer, &QTimer::timeout, [this]() { updateValue(1); });
  }

  void setButtonStyle(QPushButton &btn, const QString &text) {
    btn.setStyleSheet("QPushButton { background-color: #393939; color: #E4E4E4; border-radius: 50px; font: 500 35px; padding: 0; } QPushButton:pressed { background-color: #4a4a4a; color: #E4E4E4; }");
    btn.setText(text);
    btn.setFixedSize(150, 100);
  }

  QPushButton btnminus, btnplus;
  QLabel label;
  Params params;
  QTimer minusTimer, plusTimer;

  virtual void updateValue(int delta) = 0;
  virtual void refresh() = 0;
};

#define ParamController(className, paramName, labelText, descText, iconPath, getValueStrFunc, newValueFunc) \
class className : public ParamValueControl { \
  Q_OBJECT \
public: \
  className() : ParamValueControl(labelText, descText, iconPath) { refresh(); } \
private: \
  QMap<int, QString> wheelLabels = {{0, "Stock"}, {1, "Lexus"}, {2, "Toyota"}, {3, "Frog"}}; \
  void refresh() override { label.setText(getValueStr()); } \
  void updateValue(int delta) override { \
    int value = QString::fromStdString(params.get(paramName)).toInt(); \
    value = newValue(value + delta); \
    params.put(paramName, QString::number(value).toStdString()); \
    refresh(); \
  } \
  QString getValueStr() { getValueStrFunc } \
  int newValue(int v) { newValueFunc } \
};

ParamController(ConditionalExperimentalModeSpeed, "ConditionalExperimentalModeSpeed", "   Switch to Experimental Mode Below", "Switch to 'Experimental Mode' below this speed when there is no lead car in order to take advantage of red lights and stop signs.", "../assets/offroad/icon_blank.png",
  return QString::fromStdString(params.get("ConditionalExperimentalModeSpeed")) + "mph";,
  return std::clamp(v, 0, 99);
)

ParamController(LaneLinesWidth, "LaneLinesWidth", "   Lane Line Width", "Customize the lane lines width. Default matches the MUTCD average of 4 inches.", "../assets/offroad/icon_blank.png",
  return QString::fromStdString(params.get("LaneLinesWidth")) + " inches";,
  return std::clamp(v, 0, 24);
)

ParamController(PathEdgeWidth, "PathEdgeWidth", "   Path Edge Width", "Customize the path edge width that displays current driving statuses. Default is 20% of the total path.", "../assets/offroad/icon_blank.png",
  return QString::fromStdString(params.get("PathEdgeWidth")) + "%";,
  return std::clamp(v, 0, 100);
)

ParamController(PathWidth, "PathWidth", "   Path Width", "Customize the path width. Default matches a 2019 Lexus ES 350.", "../assets/offroad/icon_blank.png",
  return QString::number(QString::fromStdString(params.get("PathWidth")).toDouble() / 10.0) + " feet";,
  return std::max(0, v);
)

ParamController(RoadEdgesWidth, "RoadEdgesWidth", "   Road Edges Width", "Customize the road edges width. Default is 1/2 of the MUTCD average lane line width of 4 inches.", "../assets/offroad/icon_blank.png",
  return QString::fromStdString(params.get("RoadEdgesWidth")) + " inches";,
  return std::clamp(v, 0, 24);
)

ParamController(SteeringWheel, "SteeringWheel", "Steering Wheel Icon", "Replace the stock openpilot steering wheel icon with a custom icon. Requires reboot for changes to take effect. Want to submit your own steering wheel? Message me on Discord: FrogsGoMoo #6969.", "../assets/offroad/icon_openpilot.png",
  return wheelLabels[QString::fromStdString(params.get("SteeringWheel")).toInt()];,
  return (v + 4) % 4;
)
