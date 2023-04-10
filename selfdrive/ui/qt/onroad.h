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
  Q_PROPERTY(bool rotatingWheel MEMBER rotatingWheel);

public:
  explicit ExperimentalButton(QWidget *parent = 0);
  void updateState(const UIState &s);

private:
  void paintEvent(QPaintEvent *event) override;

  Params params;
  bool rotatingWheel;
  QPixmap engage_img;
  QPixmap experimental_img;
  QString wheel;
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

  Q_PROPERTY(bool blindspot_left MEMBER blindspot_left);
  Q_PROPERTY(bool blindspot_right MEMBER blindspot_right);
  Q_PROPERTY(bool compass MEMBER compass);
  Q_PROPERTY(bool frogColors MEMBER frogColors);
  Q_PROPERTY(bool frogSignals MEMBER frogSignals);
  Q_PROPERTY(bool left_on MEMBER left_on);
  Q_PROPERTY(bool muteDM MEMBER muteDM);
  Q_PROPERTY(bool right_on MEMBER right_on);
  Q_PROPERTY(bool rotatingWheel MEMBER rotatingWheel);
  Q_PROPERTY(float bearingAccuracyDeg MEMBER bearingAccuracyDeg);
  Q_PROPERTY(float bearingDeg MEMBER bearingDeg);
  Q_PROPERTY(int steering_angle_deg MEMBER steering_angle_deg);

public:
  explicit AnnotatedCameraWidget(VisionStreamType type, QWidget* parent = 0);
  void updateState(const UIState &s);

private:
  void drawCompass(QPainter &p, int x, int y, QPixmap &img, QBrush bg, float opacity, float bearing_deg = 0);
  void drawFrogSignals(QPainter &p);
  void drawIcon(QPainter &p, int x, int y, QPixmap &img, QBrush bg, float opacity);
  void drawIconRotate(QPainter &p, int x, int y, QPixmap &img, QBrush bg, float opacity, int angle_deg = 0);
  void drawText(QPainter &p, int x, int y, const QString &text, int alpha = 255);

  ExperimentalButton *experimental_btn;
  QPixmap compass_inner_img;
  QPixmap compass_outer_img;
  QPixmap dm_img;
  QPixmap engage_img;
  QPixmap experimental_img;
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
  bool blindspot_left;
  bool blindspot_right;
  bool compass;
  bool frogColors;
  bool frogSignals;
  bool left_on;
  bool muteDM;
  bool right_on;
  bool rotatingWheel;
  float bearingAccuracyDeg;
  float bearingDeg = 0;
  int status = STATUS_DISENGAGED;
  int steering_angle_deg = 0;
  QString wheel;
  std::unique_ptr<PubMaster> pm;
  std::vector<QPixmap> signal_img_vector;

  int skip_frame_count = 0;
  bool wide_cam_requested = false;

  Params params;

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
