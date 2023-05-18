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
    setupButton(btnminus, "-", -1);
    setupButton(btnplus, "+", 1);
    hlayout->addWidget(&label);
    hlayout->addWidget(&btnminus);
    hlayout->addWidget(&btnplus);
  }

  void setupButton(QPushButton &btn, const QString &text, int delta) {
    btn.setStyleSheet("QPushButton { background-color: #393939; color: #E4E4E4; border-radius: 50px; font: 500 35px; padding: 0; } QPushButton:pressed { background-color: #4a4a4a; color: #E4E4E4; }");
    btn.setText(text);
    btn.setFixedSize(150, 100);
    btn.setAutoRepeat(true);
    btn.setAutoRepeatInterval(150);
    connect(&btn, &QPushButton::clicked, [this, delta]() { updateValue(delta); });
  }

  QPushButton btnminus, btnplus;
  QLabel label;
  Params params;

  virtual void updateValue(int delta) = 0;
  virtual void refresh() = 0;
};

#define ParamControllerFloat(className, paramName, labelText, descText, iconPath, getValueStrFunc, newValueFunc) \
class className : public ParamValueControl { \
  Q_OBJECT \
public: \
  className() : ParamValueControl(labelText, descText, iconPath) { refresh(); } \
private: \
  QMap<int, QString> wheelLabels = {{0, "Stock"}, {1, "Lexus"}, {2, "Toyota"}, {3, "Frog"}, {4, "Rocket"}}; \
  void refresh() override { \
    label.setText(getValueStr()); \
    params.putBool("FrogPilotTogglesUpdated", true); \
  } \
  void updateValue(int delta) override { \
    float value = params.getFloat(paramName); \
    value = newValue(value + delta); \
    params.putFloat(paramName, value); \
    refresh(); \
  } \
  QString getValueStr() { getValueStrFunc } \
  float newValue(float v) { newValueFunc } \
};

#define ParamControllerInt(className, paramName, labelText, descText, iconPath, getValueStrFunc, newValueFunc) \
class className : public ParamValueControl { \
  Q_OBJECT \
public: \
  className() : ParamValueControl(labelText, descText, iconPath) { refresh(); } \
private: \
  QMap<int, QString> wheelLabels = {{0, "Stock"}, {1, "Lexus"}, {2, "Toyota"}, {3, "Frog"}, {4, "Rocket"}}; \
  void refresh() override { \
    label.setText(getValueStr()); \
    params.putBool("FrogPilotTogglesUpdated", true); \
  } \
  void updateValue(int delta) override { \
    int value = params.getInt(paramName); \
    value = newValue(value + delta); \
    params.putInt(paramName, value); \
    refresh(); \
  } \
  QString getValueStr() { getValueStrFunc } \
  int newValue(int v) { newValueFunc } \
};

ParamControllerInt(ConditionalExperimentalModeSpeed, "ConditionalExperimentalModeSpeed", "   Experimental Mode Below (Lead)", "Switch to 'Experimental Mode' below this speed when there is no lead car in order to take advantage of red lights and stop signs.", "../assets/offroad/icon_blank.png",
  int value = params.getInt("ConditionalExperimentalModeSpeed");
  return value == 0 ? "Off" : QString::number(value) + "mph";,
  return std::clamp(v, 0, 99);
)

ParamControllerInt(ConditionalExperimentalModeSpeedLead, "ConditionalExperimentalModeSpeedLead", "   Experimental Mode Below (No Lead)", "Switch to 'Experimental Mode' below this speed in order to take advantage of red lights and stop signs.", "../assets/offroad/icon_blank.png",
  int value = params.getInt("ConditionalExperimentalModeSpeedLead");
  return value == 0 ? "Off" : QString::number(value) + "mph";,
  return std::clamp(v, 0, 99);
)

ParamControllerInt(DeviceShutdownTimer, "DeviceShutdownTimer", "Device Shutdown Timer", "Set the timer for when the device turns off after being offroad to reduce energy waste and prevent battery drain.", "../assets/offroad/icon_time.png",
  int value = params.getInt("DeviceShutdownTimer");
  return value == 0 ? "Instant" : QString::number(value) + " hours";,
  return std::clamp(v, 0, 30);
)

ParamControllerFloat(LaneLinesWidth, "LaneLinesWidth", "   Lane Line Width", "Customize the lane lines width. Default matches the MUTCD average of 4 inches.", "../assets/offroad/icon_blank.png",
  return QString::number(params.getFloat("LaneLinesWidth")) + " inches";,
  return std::clamp(v, 0.f, 24.f);
)

ParamControllerInt(PathColorTesting, "PathColorTesting", "Path Color Testing", "Sets the color hue for testing the path color.", "../assets/offroad/icon_blank.png",
  return QString::number(params.getInt("PathColorTesting"));,
  return std::clamp(v, -1000, 1000);
)

ParamControllerInt(PathEdgeWidth, "PathEdgeWidth", "   Path Edge Width", "Customize the path edge width that displays current driving statuses. Default is 20% of the total path.", "../assets/offroad/icon_blank.png",
  return QString::number(params.getInt("PathEdgeWidth")) + "%";,
  return std::clamp(v, 0, 100);
)

ParamControllerFloat(PathWidth, "PathWidth", "   Path Width", "Customize the path width. Default matches a 2019 Lexus ES 350.", "../assets/offroad/icon_blank.png",
  return QString::number(params.getFloat("PathWidth") / 10.0) + " feet";,
  return std::clamp(v, 0.f, 100.f);
)

ParamControllerFloat(RoadEdgesWidth, "RoadEdgesWidth", "   Road Edges Width", "Customize the road edges width. Default is 1/2 of the MUTCD average lane line width of 4 inches.", "../assets/offroad/icon_blank.png",
  return QString::number(params.getFloat("RoadEdgesWidth")) + " inches";,
  return std::clamp(v, 0.f, 24.f);
)

ParamControllerInt(ScreenBrightness, "ScreenBrightness", "Screen Brightness", "Set a custom screen brightness level or use the default 'Auto' brightness setting.", "../assets/offroad/icon_light.png",
  int brightness = params.getInt("ScreenBrightness");
  return brightness == 101 ? "Auto" : brightness == 0 ? "Screen Off" : QString::number(brightness) + "%";,
  return std::clamp(v, 0, 101);
)

ParamControllerInt(SteeringWheel, "SteeringWheel", "Steering Wheel Icon", "Replace the stock openpilot steering wheel icon with a custom icon.\n\nWant to submit your own steering wheel? Message me on Discord:\nFrogsGoMoo #6969.", "../assets/offroad/icon_openpilot.png",
  return wheelLabels[params.getInt("SteeringWheel")];,
  return (v + 5) % 5;
)
