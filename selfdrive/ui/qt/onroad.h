#pragma once

#include <QPushButton>
#include <QStackedLayout>
#include <QWidget>

#include "common/util.h"
#include "selfdrive/ui/ui.h"
#include "selfdrive/ui/qt/widgets/cameraview.h"


const int btn_size = 192;
const int img_size = (btn_size / 4) * 3;


// ***** onroad widgets *****
class OnroadAlerts : public QWidget {
  Q_OBJECT

public:
  OnroadAlerts(QWidget *parent = 0) : QWidget(parent) {};
  void updateAlert(const Alert &a, const QColor &color);

protected:
  void paintEvent(QPaintEvent*) override;

private:
  QColor bg;
  Alert alert = {};
};

class ExperimentalButton : public QPushButton {
  Q_OBJECT

  // FrogPilot properties
  Q_PROPERTY(int conditionalOverridden MEMBER conditionalOverridden);
  Q_PROPERTY(int steeringWheel MEMBER steeringWheel);

public:
  explicit ExperimentalButton(QWidget *parent = 0);
  void updateState(const UIState &s);

private:
  void paintEvent(QPaintEvent *event) override;

  Params params;
  QPixmap engage_img;
  QPixmap experimental_img;

  // FrogPilot variables
  int conditionalOverridden;
  int steeringWheel;
  std::map<int, QPixmap> wheel_images;
};

// container window for the NVG UI
class AnnotatedCameraWidget : public CameraWidget {
  Q_OBJECT
  Q_PROPERTY(float speed MEMBER speed);
  Q_PROPERTY(QString speedUnit MEMBER speedUnit);
  Q_PROPERTY(float setSpeed MEMBER setSpeed);
  Q_PROPERTY(float speedLimit MEMBER speedLimit);
  Q_PROPERTY(bool is_cruise_set MEMBER is_cruise_set);
  Q_PROPERTY(bool has_eu_speed_limit MEMBER has_eu_speed_limit);
  Q_PROPERTY(bool has_us_speed_limit MEMBER has_us_speed_limit);
  Q_PROPERTY(bool is_metric MEMBER is_metric);

  Q_PROPERTY(bool dmActive MEMBER dmActive);
  Q_PROPERTY(bool hideDM MEMBER hideDM);
  Q_PROPERTY(bool rightHandDM MEMBER rightHandDM);
  Q_PROPERTY(int status MEMBER status);

  // FrogPilot properties
  Q_PROPERTY(bool adjustableFollowDistance MEMBER adjustableFollowDistance);
  Q_PROPERTY(bool adjustableFollowDistanceCar MEMBER adjustableFollowDistanceCar);
  Q_PROPERTY(bool blindspotLeft MEMBER blindspotLeft);
  Q_PROPERTY(bool blindspotRight MEMBER blindspotRight);
  Q_PROPERTY(bool compass MEMBER compass);
  Q_PROPERTY(bool conditionalExperimental MEMBER conditionalExperimental);
  Q_PROPERTY(bool experimentalMode MEMBER experimentalMode);
  Q_PROPERTY(bool frogColors MEMBER frogColors);
  Q_PROPERTY(bool frogSignals MEMBER frogSignals);
  Q_PROPERTY(bool muteDM MEMBER muteDM);
  Q_PROPERTY(bool rotatingWheel MEMBER rotatingWheel);
  Q_PROPERTY(bool turnSignalLeft MEMBER turnSignalLeft);
  Q_PROPERTY(bool turnSignalRight MEMBER turnSignalRight);
  Q_PROPERTY(int adjustableFollowDistanceProfile MEMBER adjustableFollowDistanceProfile);
  Q_PROPERTY(int bearingDeg MEMBER bearingDeg);
  Q_PROPERTY(int conditionalOverridden MEMBER conditionalOverridden);
  Q_PROPERTY(int conditionalSpeed MEMBER conditionalSpeed);
  Q_PROPERTY(int conditionalSpeedLead MEMBER conditionalSpeedLead);
  Q_PROPERTY(int conditionalStatus MEMBER conditionalStatus);
  Q_PROPERTY(int steeringAngleDeg MEMBER steeringAngleDeg);
  Q_PROPERTY(int steeringWheel MEMBER steeringWheel);

public:
  explicit AnnotatedCameraWidget(VisionStreamType type, QWidget* parent = 0);
  void updateState(const UIState &s);

private:
  void drawIcon(QPainter &p, int x, int y, QPixmap &img, QBrush bg, float opacity);
  void drawText(QPainter &p, int x, int y, const QString &text, int alpha = 255);
  
  // FrogPilot widgets
  void drawAdjustableFollowDistance(QPainter &p);
  void drawCompass(QPainter &p);
  void drawConditionalExperimentalStatus(QPainter &p);
  void drawFrogSignals(QPainter &p);
  void drawRotatingWheel(QPainter &p, int x, int y);

  ExperimentalButton *experimental_btn;
  QPixmap dm_img;
  float speed;
  QString speedUnit;
  float setSpeed;
  float speedLimit;
  bool is_cruise_set = false;
  bool is_metric = false;
  bool dmActive = false;
  bool hideDM = false;
  bool rightHandDM = false;
  float dm_fade_state = 1.0;
  bool has_us_speed_limit = false;
  bool has_eu_speed_limit = false;
  bool v_ego_cluster_seen = false;
  int status = STATUS_DISENGAGED;
  std::unique_ptr<PubMaster> pm;

  int skip_frame_count = 0;
  bool wide_cam_requested = false;

  // FrogPilot variables
  bool adjustableFollowDistance;
  bool adjustableFollowDistanceCar;
  bool blindspotLeft;
  bool blindspotRight;
  bool compass;
  bool conditionalExperimental;
  bool experimentalMode;
  bool frogColors;
  bool frogSignals;
  bool muteDM;
  bool rotatingWheel;
  bool turnSignalLeft;
  bool turnSignalRight;
  int adjustableFollowDistanceProfile;
  int animationFrameIndex;
  int bearingDeg;
  int conditionalOverridden;
  int conditionalSpeed;
  int conditionalSpeedLead;
  int conditionalStatus;
  int steeringAngleDeg;
  int steeringWheel;
  QPixmap compass_inner_img;
  QPixmap engage_img;
  QPixmap experimental_img;
  QVector<std::pair<QPixmap, QString>> profile_data;
  static constexpr int totalFrames = 8;
  std::map<int, QPixmap> wheel_images;
  std::vector<QPixmap> signalImgVector;

protected:
  void paintGL() override;
  void initializeGL() override;
  void showEvent(QShowEvent *event) override;
  void updateFrameMat() override;
  void drawLaneLines(QPainter &painter, const UIState *s);
  void drawLead(QPainter &painter, const cereal::RadarState::LeadData::Reader &lead_data, const QPointF &vd);
  void drawHud(QPainter &p);
  void drawDriverState(QPainter &painter, const UIState *s);
  inline QColor redColor(int alpha = 255) { return QColor(201, 34, 49, alpha); }
  inline QColor whiteColor(int alpha = 255) { return QColor(255, 255, 255, alpha); }
  inline QColor blackColor(int alpha = 255) { return QColor(0, 0, 0, alpha); }
  
  // FrogPilot colors
  inline QColor frogColor(int alpha = 242) { return QColor(23, 134, 68, alpha); }

  double prev_draw_t = 0;
  FirstOrderFilter fps_filter;
};

// container for all onroad widgets
class OnroadWindow : public QWidget {
  Q_OBJECT

public:
  OnroadWindow(QWidget* parent = 0);
  bool isMapVisible() const { return map && map->isVisible(); }

private:
  void paintEvent(QPaintEvent *event);
  void mousePressEvent(QMouseEvent* e) override;
  OnroadAlerts *alerts;
  AnnotatedCameraWidget *nvg;
  QColor bg = bg_colors[STATUS_DISENGAGED];
  QWidget *map = nullptr;
  QHBoxLayout* split;

private slots:
  void offroadTransition(bool offroad);
  void updateState(const UIState &s);
};
