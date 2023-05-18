#include "selfdrive/ui/qt/onroad.h"

#include <chrono>

#include <cmath>

#include <QDebug>

#include <QTimer>
#include <QElapsedTimer>

#include "common/timing.h"
#include "selfdrive/ui/qt/util.h"
#ifdef ENABLE_MAPS
#include "selfdrive/ui/qt/maps/map.h"
#include "selfdrive/ui/qt/maps/map_helpers.h"
#endif

OnroadWindow::OnroadWindow(QWidget *parent) : QWidget(parent) {
  QVBoxLayout *main_layout  = new QVBoxLayout(this);
  main_layout->setMargin(bdr_s);
  QStackedLayout *stacked_layout = new QStackedLayout;
  stacked_layout->setStackingMode(QStackedLayout::StackAll);
  main_layout->addLayout(stacked_layout);

  nvg = new AnnotatedCameraWidget(VISION_STREAM_ROAD, this);

  QWidget * split_wrapper = new QWidget;
  split = new QHBoxLayout(split_wrapper);
  split->setContentsMargins(0, 0, 0, 0);
  split->setSpacing(0);
  split->addWidget(nvg);

  if (getenv("DUAL_CAMERA_VIEW")) {
    CameraWidget *arCam = new CameraWidget("camerad", VISION_STREAM_ROAD, true, this);
    split->insertWidget(0, arCam);
  }

  if (getenv("MAP_RENDER_VIEW")) {
    CameraWidget *map_render = new CameraWidget("navd", VISION_STREAM_MAP, false, this);
    split->insertWidget(0, map_render);
  }

  stacked_layout->addWidget(split_wrapper);

  alerts = new OnroadAlerts(this);
  alerts->setAttribute(Qt::WA_TransparentForMouseEvents, true);
  stacked_layout->addWidget(alerts);

  // setup stacking order
  alerts->raise();

  setAttribute(Qt::WA_OpaquePaintEvent);
  QObject::connect(uiState(), &UIState::uiUpdate, this, &OnroadWindow::updateState);
  QObject::connect(uiState(), &UIState::offroadTransition, this, &OnroadWindow::offroadTransition);
}

void OnroadWindow::updateState(const UIState &s) {
  QColor bgColor = bg_colors[s.status];
  Alert alert = Alert::get(*(s.sm), s.scene.started_frame);
  if (s.sm->updated("controlsState") || !alert.equal({})) {
    if (alert.type == "controlsUnresponsive") {
      bgColor = bg_colors[STATUS_ALERT];
    } else if (alert.type == "controlsUnresponsivePermanent") {
      bgColor = bg_colors[STATUS_DISENGAGED];
    }
    alerts->updateAlert(alert, bgColor);
  }

  if (s.scene.map_on_left) {
    split->setDirection(QBoxLayout::LeftToRight);
  } else {
    split->setDirection(QBoxLayout::RightToLeft);
  }

  nvg->updateState(s);

  if (bg != bgColor) {
    // repaint border
    bg = bgColor;
    update();
  }
}

void OnroadWindow::mousePressEvent(QMouseEvent* e) {
  if (map != nullptr) {
    bool sidebarVisible = geometry().x() > 0;
    map->setVisible(!sidebarVisible && !map->isVisible());
  }

  // FrogPilot clickable widgets
  const auto &scene = uiState()->scene;
  static auto params = Params();
  static bool propagateEvent = true;
  static bool recentlyTapped = false;
  const bool isAdjustableFollow = scene.adjustable_follow_distance && !scene.adjustable_follow_distance_car;
  const bool isExperimentalModewheel = scene.experimental_mode_via_wheel && !scene.steering_wheel_car;
  const int x_offset = scene.mute_dm ? 50 : 250;
  const SubMaster &sm = *uiState()->sm;
  static bool rightHandDM = false;

  // Update at 2Hz
  if (sm.frame % (UI_FREQ / 2) == 0) {
    rightHandDM = sm["driverMonitoringState"].getDriverMonitoringState().getIsRHD();
  }

  // Adjustable follow distance button
  int x = rightHandDM ? rect().right() - (btn_size - 24) / 2 - (bdr_s * 2) - x_offset : (btn_size - 24) / 2 + (bdr_s * 2) + x_offset;
  const int y = rect().bottom() - (scene.conditional_experimental ? 20 : 0) - footer_h / 2;
  QPoint adjustableFollowDistanceCenter(x, y);
  // Give the button a 25% offset so it doesn't need to be clicked on perfectly
  constexpr int adjustableFollowDistanceRadius = btn_size * 1.25;
  bool isAdjustableFollowDistanceClicked = (e->pos() - adjustableFollowDistanceCenter).manhattanLength() <= adjustableFollowDistanceRadius;

  // Check if the button was clicked and if adjustableFollowDistances is toggled on
  if (isAdjustableFollowDistanceClicked && isAdjustableFollow) {
    params.putInt("AdjustableFollowDistanceProfile", (scene.adjustable_follow_distance_profile % 3) + 1);
    propagateEvent = false;
  // If the click wasn't on the button for adjustableFollowDistances, change the value of "ExperimentalMode" and "ExperimentalModeOverride"
  } else if (recentlyTapped && isExperimentalModewheel) {
    bool experimentalMode = params.getBool("ExperimentalMode");
    if (scene.conditional_experimental) {
      params.putInt("ExperimentalModeOverride", scene.experimental_mode_override != 0 ? 0 : experimentalMode ? 1 : 2);
    } else {
      params.putBool("ExperimentalMode", !experimentalMode);
    }
    recentlyTapped = false;
  } else {
    recentlyTapped = true;
  }

  // propagation event to parent(HomeWindow)
  if (propagateEvent) {
    QWidget::mousePressEvent(e);
  }
}

void OnroadWindow::offroadTransition(bool offroad) {
#ifdef ENABLE_MAPS
  if (!offroad) {
    if (map == nullptr && (uiState()->primeType() || !MAPBOX_TOKEN.isEmpty())) {
      MapWindow * m = new MapWindow(get_mapbox_settings());
      map = m;

      QObject::connect(uiState(), &UIState::offroadTransition, m, &MapWindow::offroadTransition);

      m->setFixedWidth(topWidget(this)->width() / 2);
      split->insertWidget(0, m);

      // Make map visible after adding to split
      m->offroadTransition(offroad);
    }
  }
#endif

  alerts->updateAlert({}, bg);
}

void OnroadWindow::paintEvent(QPaintEvent *event) {
  QPainter p(this);
  p.fillRect(rect(), QColor(bg.red(), bg.green(), bg.blue(), 255));
}

// ***** onroad widgets *****

// OnroadAlerts
void OnroadAlerts::updateAlert(const Alert &a, const QColor &color) {
  if (!alert.equal(a) || color != bg) {
    alert = a;
    bg = color;
    update();
  }
}

void OnroadAlerts::paintEvent(QPaintEvent *event) {
  if (alert.size == cereal::ControlsState::AlertSize::NONE) {
    return;
  }
  static std::map<cereal::ControlsState::AlertSize, const int> alert_sizes = {
    {cereal::ControlsState::AlertSize::SMALL, 271},
    {cereal::ControlsState::AlertSize::MID, 420},
    {cereal::ControlsState::AlertSize::FULL, height()},
  };
  int h = alert_sizes[alert.size];
  QRect r = QRect(0, height() - h, width(), h);

  QPainter p(this);

  // draw background + gradient
  p.setPen(Qt::NoPen);
  p.setCompositionMode(QPainter::CompositionMode_SourceOver);

  p.setBrush(QBrush(bg));
  p.drawRect(r);

  QLinearGradient g(0, r.y(), 0, r.bottom());
  g.setColorAt(0, QColor::fromRgbF(0, 0, 0, 0.05));
  g.setColorAt(1, QColor::fromRgbF(0, 0, 0, 0.35));

  p.setCompositionMode(QPainter::CompositionMode_DestinationOver);
  p.setBrush(QBrush(g));
  p.fillRect(r, g);
  p.setCompositionMode(QPainter::CompositionMode_SourceOver);

  // text
  const QPoint c = r.center();
  p.setPen(QColor(0xff, 0xff, 0xff));
  p.setRenderHint(QPainter::TextAntialiasing);
  if (alert.size == cereal::ControlsState::AlertSize::SMALL) {
    configFont(p, "Inter", 74, "SemiBold");
    p.drawText(r, Qt::AlignCenter, alert.text1);
  } else if (alert.size == cereal::ControlsState::AlertSize::MID) {
    configFont(p, "Inter", 88, "Bold");
    p.drawText(QRect(0, c.y() - 125, width(), 150), Qt::AlignHCenter | Qt::AlignTop, alert.text1);
    configFont(p, "Inter", 66, "Regular");
    p.drawText(QRect(0, c.y() + 21, width(), 90), Qt::AlignHCenter, alert.text2);
  } else if (alert.size == cereal::ControlsState::AlertSize::FULL) {
    bool l = alert.text1.length() > 15;
    configFont(p, "Inter", l ? 132 : 177, "Bold");
    p.drawText(QRect(0, r.y() + (l ? 240 : 270), width(), 600), Qt::AlignHCenter | Qt::TextWordWrap, alert.text1);
    configFont(p, "Inter", 88, "Regular");
    p.drawText(QRect(0, r.height() - (l ? 361 : 420), width(), 300), Qt::AlignHCenter | Qt::TextWordWrap, alert.text2);
  }
}


ExperimentalButton::ExperimentalButton(QWidget *parent) : QPushButton(parent) {
  setVisible(false);
  setFixedSize(btn_size, btn_size);
  setCheckable(true);

  params = Params();
  engage_img = loadPixmap("../assets/img_chffr_wheel.png", {img_size, img_size});
  experimental_img = loadPixmap("../assets/img_experimental.svg", {img_size, img_size});

  // Custom steering wheel images
  wheel_images = {
    {0, loadPixmap("../assets/img_chffr_wheel.png", {img_size, img_size})},
    {1, loadPixmap("../assets/lexus.png", {img_size, img_size})},
    {2, loadPixmap("../assets/toyota.png", {img_size, img_size})},
    {3, loadPixmap("../assets/frog.png", {img_size, img_size})},
    {4, loadPixmap("../assets/rocket.png", {img_size, img_size})}
  };

  QObject::connect(this, &QPushButton::toggled, [=](bool checked) {
    params.putBool("ExperimentalMode", checked);
  });
}

void ExperimentalButton::updateState(const UIState &s) {
  const SubMaster &sm = *(s.sm);

  // button is "visible" if engageable or enabled
  const auto cs = sm["controlsState"].getControlsState();
  setVisible(cs.getEngageable() || cs.getEnabled());

  // button is "checked" if experimental mode is enabled
  setChecked(sm["controlsState"].getControlsState().getExperimentalMode());

  // disable button when experimental mode is not available, or has not been confirmed for the first time
  const auto cp = sm["carParams"].getCarParams();
  const bool experimental_mode_available = cp.getExperimentalLongitudinalAvailable() ? params.getBool("ExperimentalLongitudinalEnabled") : cp.getOpenpilotLongitudinalControl();
  setEnabled(params.getBool("ExperimentalModeConfirmed") && experimental_mode_available);
  
  // FrogPilot properties
  setProperty("steeringWheel", s.scene.steering_wheel);
}

void ExperimentalButton::paintEvent(QPaintEvent *event) {
  // If the rotating steering wheel toggle is on hide the icon
  static auto &scene = uiState()->scene;
  if (!scene.rotating_wheel) {
    // Custom steering wheel icon
    engage_img = wheel_images[steeringWheel];

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    QPoint center(btn_size / 2, btn_size / 2);
    QPixmap img = steeringWheel ? engage_img : (isChecked() ? experimental_img : engage_img);

    p.setOpacity(1.0);
    p.setPen(Qt::NoPen);
    p.setBrush(scene.conditional_status || scene.conditional_overridden ? QColor(255, 246, 0, 255) : steeringWheel && isChecked() ? QColor(218, 111, 37, 241) : QColor(0, 0, 0, 166));
    p.drawEllipse(center, btn_size / 2, btn_size / 2);
    p.setOpacity(isDown() ? 0.8 : 1.0);
    p.drawPixmap((btn_size - img_size) / 2, (btn_size - img_size) / 2, img);
  }
}


AnnotatedCameraWidget::AnnotatedCameraWidget(VisionStreamType type, QWidget* parent) : fps_filter(UI_FREQ, 3, 1. / UI_FREQ), CameraWidget("camerad", type, true, parent) {
  pm = std::make_unique<PubMaster, const std::initializer_list<const char *>>({"uiDebug"});

  QVBoxLayout *main_layout  = new QVBoxLayout(this);
  main_layout->setMargin(bdr_s);
  main_layout->setSpacing(0);

  experimental_btn = new ExperimentalButton(this);
  main_layout->addWidget(experimental_btn, 0, Qt::AlignTop | Qt::AlignRight);

  compass_inner_img = loadPixmap("../assets/images/compass_inner.png", {img_size, img_size});
  dm_img = loadPixmap("../assets/img_driver_face.png", {img_size + 5, img_size + 5});
  engage_img = loadPixmap("../assets/img_chffr_wheel.png", {img_size, img_size});
  experimental_img = loadPixmap("../assets/img_experimental.svg", {img_size, img_size});
  
  // Custom steering wheel images
  wheel_images = {
    {0, loadPixmap("../assets/img_chffr_wheel.png", {img_size, img_size})},
    {1, loadPixmap("../assets/lexus.png", {img_size, img_size})},
    {2, loadPixmap("../assets/toyota.png", {img_size, img_size})},
    {3, loadPixmap("../assets/frog.png", {img_size, img_size})},
    {4, loadPixmap("../assets/rocket.png", {img_size, img_size})}
  };
  
  // Following distance profiles
  profile_data = {
    {QPixmap("../assets/aggressive.png"), "Aggressive"},
    {QPixmap("../assets/comfort.png"), "Comfort"},
    {QPixmap("../assets/relaxed.png"), "Relaxed"}
  };

  // Turn signal images
  const QStringList imagePaths = {
    "../assets/images/frog_turn_signal_1.png",
    "../assets/images/frog_turn_signal_2.png",
    "../assets/images/frog_turn_signal_3.png",
    "../assets/images/frog_turn_signal_4.png"
  };
  for (int i = 0; i < 2; ++i) {
    for (const QString& path : imagePaths) {
      signalImgVector.push_back(QPixmap(path));
    }
  }
  // Add the blindspot signal image to the vector
  signalImgVector.push_back(QPixmap("../assets/images/frog_turn_signal_1_red.png"));
  // Initialize the timer for the turn signal animation
  QTimer *animationTimer;
  animationTimer = new QTimer(this);
  connect(animationTimer, &QTimer::timeout, this, [this] {
    animationFrameIndex = (animationFrameIndex + 1) % totalFrames;
    update();
  });
  animationTimer->start(totalFrames * 11); // 11 * totalFrames (88) milliseconds per frame; syncs up perfectly with my 2019 Lexus ES 350 turn signal clicks
}

void AnnotatedCameraWidget::updateState(const UIState &s) {
  const int SET_SPEED_NA = 255;
  const SubMaster &sm = *(s.sm);

  const bool cs_alive = sm.alive("controlsState");
  const bool nav_alive = sm.alive("navInstruction") && sm["navInstruction"].getValid();

  const auto cs = sm["controlsState"].getControlsState();

  // Handle older routes where vCruiseCluster is not set
  float v_cruise =  cs.getVCruiseCluster() == 0.0 ? cs.getVCruise() : cs.getVCruiseCluster();
  float set_speed = cs_alive ? v_cruise : SET_SPEED_NA;
  bool cruise_set = set_speed > 0 && (int)set_speed != SET_SPEED_NA;
  if (cruise_set && !s.scene.is_metric) {
    set_speed *= KM_TO_MILE;
  }

  // Handle older routes where vEgoCluster is not set
  float v_ego;
  if (sm["carState"].getCarState().getVEgoCluster() == 0.0 && !v_ego_cluster_seen) {
    v_ego = sm["carState"].getCarState().getVEgo();
  } else {
    v_ego = sm["carState"].getCarState().getVEgoCluster();
    v_ego_cluster_seen = true;
  }
  float cur_speed = cs_alive ? std::max<float>(0.0, v_ego) : 0.0;
  cur_speed *= s.scene.is_metric ? MS_TO_KPH : MS_TO_MPH;

  auto speed_limit_sign = sm["navInstruction"].getNavInstruction().getSpeedLimitSign();
  float speed_limit = nav_alive ? sm["navInstruction"].getNavInstruction().getSpeedLimit() : 0.0;
  speed_limit *= (s.scene.is_metric ? MS_TO_KPH : MS_TO_MPH);

  setProperty("speedLimit", speed_limit);
  setProperty("has_us_speed_limit", nav_alive && speed_limit_sign == cereal::NavInstruction::SpeedLimitSign::MUTCD);
  setProperty("has_eu_speed_limit", nav_alive && speed_limit_sign == cereal::NavInstruction::SpeedLimitSign::VIENNA);

  setProperty("is_cruise_set", cruise_set);
  setProperty("is_metric", s.scene.is_metric);
  setProperty("speed", cur_speed);
  setProperty("setSpeed", set_speed);
  setProperty("speedUnit", s.scene.is_metric ? tr("km/h") : tr("mph"));
  setProperty("hideDM", (cs.getAlertSize() != cereal::ControlsState::AlertSize::NONE));
  setProperty("status", s.status);

  // update engageability/experimental mode button
  experimental_btn->updateState(s);

  // update DM icons at 2Hz
  if (sm.frame % (UI_FREQ / 2) == 0) {
    setProperty("dmActive", sm["driverMonitoringState"].getDriverMonitoringState().getIsActiveMode());
    setProperty("rightHandDM", sm["driverMonitoringState"].getDriverMonitoringState().getIsRHD());
  }

  // DM icon transition
  dm_fade_state = fmax(0.0, fmin(1.0, dm_fade_state+0.2*(0.5-(float)(dmActive))));

  // FrogPilot properties
  setProperty("adjustableFollowDistance", s.scene.adjustable_follow_distance);
  setProperty("adjustableFollowDistanceCar", s.scene.adjustable_follow_distance_car);
  setProperty("adjustableFollowDistanceProfile", s.scene.adjustable_follow_distance_profile);
  setProperty("bearingDeg", s.scene.bearing_deg);
  setProperty("blindspotLeft", s.scene.blindspot_left);
  setProperty("blindspotRight", s.scene.blindspot_right);
  setProperty("compass", s.scene.compass);
  setProperty("conditionalExperimental", s.scene.conditional_experimental);
  setProperty("conditionalOverridden", s.scene.conditional_overridden);
  setProperty("conditionalSpeed", s.scene.conditional_speed);
  setProperty("conditionalSpeedLead", s.scene.conditional_speed_lead);
  setProperty("conditionalStatus", s.scene.conditional_status);
  setProperty("experimentalMode", s.scene.experimental_mode);
  setProperty("frogColors", s.scene.frog_colors);
  setProperty("frogSignals", s.scene.frog_signals);
  setProperty("muteDM", s.scene.mute_dm);
  setProperty("rotatingWheel", s.scene.rotating_wheel);
  setProperty("steeringAngleDeg", s.scene.steering_angle_deg);
  setProperty("steeringWheel", s.scene.steering_wheel);
  setProperty("turnSignalLeft", s.scene.turn_signal_left);
  setProperty("turnSignalRight", s.scene.turn_signal_right);
}

void AnnotatedCameraWidget::drawHud(QPainter &p) {
  p.save();

  // Header gradient
  QLinearGradient bg(0, header_h - (header_h / 2.5), 0, header_h);
  bg.setColorAt(0, QColor::fromRgbF(0, 0, 0, 0.45));
  bg.setColorAt(1, QColor::fromRgbF(0, 0, 0, 0));
  p.fillRect(0, 0, width(), header_h, bg);

  QString speedLimitStr = (speedLimit > 1) ? QString::number(std::nearbyint(speedLimit)) : "–";
  QString speedStr = QString::number(std::nearbyint(speed));
  QString setSpeedStr = is_cruise_set ? QString::number(std::nearbyint(setSpeed)) : "–";

  // Draw outer box + border to contain set speed and speed limit
  int default_rect_width = 172;
  int rect_width = default_rect_width;
  if (is_metric || has_eu_speed_limit) rect_width = 200;
  if (has_us_speed_limit && speedLimitStr.size() >= 3) rect_width = 223;

  int rect_height = 204;
  if (has_us_speed_limit) rect_height = 402;
  else if (has_eu_speed_limit) rect_height = 392;

  int top_radius = 32;
  int bottom_radius = has_eu_speed_limit ? 100 : 32;

  QRect set_speed_rect(60 + default_rect_width / 2 - rect_width / 2, 45, rect_width, rect_height);
  p.setPen(QPen(whiteColor(75), 6));
  p.setBrush(blackColor(166));
  drawRoundedRect(p, set_speed_rect, top_radius, top_radius, bottom_radius, bottom_radius);

  // Draw MAX
  if (is_cruise_set) {
    if (status == STATUS_DISENGAGED) {
      p.setPen(whiteColor());
    } else if (status == STATUS_OVERRIDE) {
      p.setPen(QColor(0x91, 0x9b, 0x95, 0xff));
    } else if (speedLimit > 0) {
      p.setPen(interpColor(
        setSpeed,
        {speedLimit + 5, speedLimit + 15, speedLimit + 25},
        {QColor(0x80, 0xd8, 0xa6, 0xff), QColor(0xff, 0xe4, 0xbf, 0xff), QColor(0xff, 0xbf, 0xbf, 0xff)}
      ));
    } else {
      p.setPen(QColor(0x80, 0xd8, 0xa6, 0xff));
    }
  } else {
    p.setPen(QColor(0xa6, 0xa6, 0xa6, 0xff));
  }
  configFont(p, "Inter", 40, "SemiBold");
  QRect max_rect = getTextRect(p, Qt::AlignCenter, tr("MAX"));
  max_rect.moveCenter({set_speed_rect.center().x(), 0});
  max_rect.moveTop(set_speed_rect.top() + 27);
  p.drawText(max_rect, Qt::AlignCenter, tr("MAX"));

  // Draw set speed
  if (is_cruise_set) {
    if (speedLimit > 0 && status != STATUS_DISENGAGED && status != STATUS_OVERRIDE) {
      p.setPen(interpColor(
        setSpeed,
        {speedLimit + 5, speedLimit + 15, speedLimit + 25},
        {whiteColor(), QColor(0xff, 0x95, 0x00, 0xff), QColor(0xff, 0x00, 0x00, 0xff)}
      ));
    } else {
      p.setPen(whiteColor());
    }
  } else {
    p.setPen(QColor(0x72, 0x72, 0x72, 0xff));
  }
  configFont(p, "Inter", 90, "Bold");
  QRect speed_rect = getTextRect(p, Qt::AlignCenter, setSpeedStr);
  speed_rect.moveCenter({set_speed_rect.center().x(), 0});
  speed_rect.moveTop(set_speed_rect.top() + 77);
  p.drawText(speed_rect, Qt::AlignCenter, setSpeedStr);



  // US/Canada (MUTCD style) sign
  if (has_us_speed_limit) {
    const int border_width = 6;
    const int sign_width = rect_width - 24;
    const int sign_height = 186;

    // White outer square
    QRect sign_rect_outer(set_speed_rect.left() + 12, set_speed_rect.bottom() - 11 - sign_height, sign_width, sign_height);
    p.setPen(Qt::NoPen);
    p.setBrush(whiteColor());
    p.drawRoundedRect(sign_rect_outer, 24, 24);

    // Smaller white square with black border
    QRect sign_rect(sign_rect_outer.left() + 1.5 * border_width, sign_rect_outer.top() + 1.5 * border_width, sign_width - 3 * border_width, sign_height - 3 * border_width);
    p.setPen(QPen(blackColor(), border_width));
    p.setBrush(whiteColor());
    p.drawRoundedRect(sign_rect, 16, 16);

    // "SPEED"
    configFont(p, "Inter", 28, "SemiBold");
    QRect text_speed_rect = getTextRect(p, Qt::AlignCenter, tr("SPEED"));
    text_speed_rect.moveCenter({sign_rect.center().x(), 0});
    text_speed_rect.moveTop(sign_rect_outer.top() + 22);
    p.drawText(text_speed_rect, Qt::AlignCenter, tr("SPEED"));

    // "LIMIT"
    QRect text_limit_rect = getTextRect(p, Qt::AlignCenter, tr("LIMIT"));
    text_limit_rect.moveCenter({sign_rect.center().x(), 0});
    text_limit_rect.moveTop(sign_rect_outer.top() + 51);
    p.drawText(text_limit_rect, Qt::AlignCenter, tr("LIMIT"));

    // Speed limit value
    configFont(p, "Inter", 70, "Bold");
    QRect speed_limit_rect = getTextRect(p, Qt::AlignCenter, speedLimitStr);
    speed_limit_rect.moveCenter({sign_rect.center().x(), 0});
    speed_limit_rect.moveTop(sign_rect_outer.top() + 85);
    p.drawText(speed_limit_rect, Qt::AlignCenter, speedLimitStr);
  }

  // EU (Vienna style) sign
  if (has_eu_speed_limit) {
    int outer_radius = 176 / 2;
    int inner_radius_1 = outer_radius - 6; // White outer border
    int inner_radius_2 = inner_radius_1 - 20; // Red circle

    // Draw white circle with red border
    QPoint center(set_speed_rect.center().x() + 1, set_speed_rect.top() + 204 + outer_radius);
    p.setPen(Qt::NoPen);
    p.setBrush(whiteColor());
    p.drawEllipse(center, outer_radius, outer_radius);
    p.setBrush(QColor(255, 0, 0, 255));
    p.drawEllipse(center, inner_radius_1, inner_radius_1);
    p.setBrush(whiteColor());
    p.drawEllipse(center, inner_radius_2, inner_radius_2);

    // Speed limit value
    int font_size = (speedLimitStr.size() >= 3) ? 60 : 70;
    configFont(p, "Inter", font_size, "Bold");
    QRect speed_limit_rect = getTextRect(p, Qt::AlignCenter, speedLimitStr);
    speed_limit_rect.moveCenter(center);
    p.setPen(blackColor());
    p.drawText(speed_limit_rect, Qt::AlignCenter, speedLimitStr);
  }

  // current speed
  configFont(p, "Inter", 176, "Bold");
  drawText(p, rect().center().x(), 210, speedStr);
  configFont(p, "Inter", 66, "Regular");
  drawText(p, rect().center().x(), 290, speedUnit, 200);

  p.restore();

  // Adjustable following distance button - Hide the button when the turn signal animation is on
  if (adjustableFollowDistance && !adjustableFollowDistanceCar && (!frogSignals || (frogSignals && !turnSignalLeft && !turnSignalRight))) {
    drawAdjustableFollowDistance(p);
  }

  // Compass - Hide the compass when the turn signal animation is on
  if (compass && (!frogSignals || (frogSignals && !turnSignalLeft && !turnSignalRight))) {
    drawCompass(p);
  }

  // Conditional experimental mode status bar
  if (conditionalExperimental) {
    drawConditionalExperimentalStatus(p);
  }

  // Frog animated turn signals
  if (frogSignals) {
    drawFrogSignals(p);
  }

  // Rotating steering wheel
  if (rotatingWheel) {
    drawRotatingWheel(p, rect().right() - btn_size / 2 - bdr_s * 2 + 25, btn_size / 2 + int(bdr_s * 1.5) - 20);
  }
}


// Window that shows camera view and variety of
// info drawn on top

void AnnotatedCameraWidget::drawText(QPainter &p, int x, int y, const QString &text, int alpha) {
  QRect real_rect = getTextRect(p, 0, text);
  real_rect.moveCenter({x, y - real_rect.height() / 2});

  p.setPen(QColor(0xff, 0xff, 0xff, alpha));
  p.drawText(real_rect.x(), real_rect.bottom(), text);
}

void AnnotatedCameraWidget::drawIcon(QPainter &p, int x, int y, QPixmap &img, QBrush bg, float opacity) {
  p.setOpacity(1.0);  // bg dictates opacity of ellipse
  p.setPen(Qt::NoPen);
  p.setBrush(bg);
  p.drawEllipse(x - btn_size / 2, y - btn_size / 2, btn_size, btn_size);
  p.setOpacity(opacity);
  p.drawPixmap(x - img.size().width() / 2, y - img.size().height() / 2, img);
  p.setOpacity(1.0);
}


void AnnotatedCameraWidget::initializeGL() {
  CameraWidget::initializeGL();
  qInfo() << "OpenGL version:" << QString((const char*)glGetString(GL_VERSION));
  qInfo() << "OpenGL vendor:" << QString((const char*)glGetString(GL_VENDOR));
  qInfo() << "OpenGL renderer:" << QString((const char*)glGetString(GL_RENDERER));
  qInfo() << "OpenGL language version:" << QString((const char*)glGetString(GL_SHADING_LANGUAGE_VERSION));

  prev_draw_t = millis_since_boot();
  setBackgroundColor(bg_colors[STATUS_DISENGAGED]);
}

void AnnotatedCameraWidget::updateFrameMat() {
  CameraWidget::updateFrameMat();
  UIState *s = uiState();
  int w = width(), h = height();

  s->fb_w = w;
  s->fb_h = h;

  // Apply transformation such that video pixel coordinates match video
  // 1) Put (0, 0) in the middle of the video
  // 2) Apply same scaling as video
  // 3) Put (0, 0) in top left corner of video
  s->car_space_transform.reset();
  s->car_space_transform.translate(w / 2 - x_offset, h / 2 - y_offset)
      .scale(zoom, zoom)
      .translate(-intrinsic_matrix.v[2], -intrinsic_matrix.v[5]);
}

void AnnotatedCameraWidget::drawLaneLines(QPainter &painter, const UIState *s) {
  painter.save();

  const UIScene &scene = s->scene;
  SubMaster &sm = *(s->sm);

  // lanelines
  for (int i = 0; i < std::size(scene.lane_line_vertices); ++i) {
    painter.setBrush(frogColors ? QColor(0x17, 0x86, 0x44, 0xf1) : QColor::fromRgbF(1.0, 1.0, 1.0, std::clamp<float>(scene.lane_line_probs[i], 0.0, 0.7)));
    painter.drawPolygon(scene.lane_line_vertices[i]);
  }

  // road edges
  for (int i = 0; i < std::size(scene.road_edge_vertices); ++i) {
    painter.setBrush(frogColors ? QColor(0x17, 0x86, 0x44, 0xf1) : QColor::fromRgbF(1.0, 0, 0, std::clamp<float>(1.0 - scene.road_edge_stds[i], 0.0, 1.0)));
    painter.drawPolygon(scene.road_edge_vertices[i]);
  }

  // paint path
  QLinearGradient bg(0, height(), 0, 0);
  if (experimentalMode || frogColors) {
    // The first half of track_vertices are the points for the right side of the path
    // and the indices match the positions of accel from uiPlan
    const auto &acceleration = sm["uiPlan"].getUiPlan().getAccel();
    const int max_len = std::min<int>(scene.track_vertices.length() / 2, acceleration.size());

    for (int i = 0; i < max_len; ++i) {
      // Some points are out of frame
      if (scene.track_vertices[i].y() < 0 || scene.track_vertices[i].y() > height()) continue;

      // Flip so 0 is bottom of frame
      float lin_grad_point = (height() - scene.track_vertices[i].y()) / height();

    float path_hue, saturation, lightness, alpha;

    if (frogColors) {
      path_hue = fmax(fmin(60 + acceleration[i] * 35, 120), 0);
      path_hue = int(path_hue * 100 + 0.5) / 100;

      saturation = fmin(fabs(acceleration[i] * 1.5), 1);
      lightness = util::map_val(saturation, 0.0f, 1.0f, 0.95f, 0.62f);
      alpha = util::map_val(lin_grad_point, 0.75f / 2.f, 0.75f, 0.4f, 0.0f);
    } else {
      // speed up: 120, slow down: 0
      path_hue = fmax(fmin(60 + acceleration[i] * 35, 120), 0);
      // FIXME: painter.drawPolygon can be slow if hue is not rounded
      path_hue = int(path_hue * 100 + 0.5) / 100;

      saturation = fmin(fabs(acceleration[i] * 1.5), 1);
      lightness = util::map_val(saturation, 0.0f, 1.0f, 0.95f, 0.62f);  // lighter when grey
      alpha = util::map_val(lin_grad_point, 0.75f / 2.f, 0.75f, 0.4f, 0.0f);  // matches previous alpha fade
    }

      bg.setColorAt(lin_grad_point, QColor::fromHslF(path_hue / 360., saturation, lightness, alpha));

      // Skip a point, unless next is last
      i += (i + 2) < max_len ? 1 : 0;
    }

  } else {
    bg.setColorAt(0.0, QColor::fromHslF(148 / 360., 0.94, 0.51, 0.4));
    bg.setColorAt(0.5, QColor::fromHslF(112 / 360., 1.0, 0.68, 0.35));
    bg.setColorAt(1.0, QColor::fromHslF(112 / 360., 1.0, 0.68, 0.0));
  }

  painter.setBrush(bg);
  painter.drawPolygon(scene.track_vertices);

  // Create new path with track vertices and track edge vertices
  QPainterPath path;
  path.addPolygon(scene.track_vertices);
  path.addPolygon(scene.track_edge_vertices);

  // Paint path edges
  QLinearGradient pe(0, height(), 0, height() / 4);
  if (conditionalStatus == 1) {
    pe.setColorAt(0.0, QColor::fromHslF(58 / 360., 1.0, 0.50, 1.0));
    pe.setColorAt(0.5, QColor::fromHslF(58 / 360., 1.0, 0.50, 0.5));
    pe.setColorAt(1.0, QColor::fromHslF(58 / 360., 1.0, 0.50, 0.1));
  } else if (experimentalMode) {
    pe.setColorAt(0.0, QColor::fromHslF(25 / 360., 0.71, 0.50, 1.0));
    pe.setColorAt(0.5, QColor::fromHslF(25 / 360., 0.71, 0.50, 0.5));
    pe.setColorAt(1.0, QColor::fromHslF(25 / 360., 0.71, 0.50, 0.1));
  } else if (frogColors) {
    pe.setColorAt(0.0, QColor::fromHslF(144 / 360., 0.71, 0.31, 1.0));
    pe.setColorAt(0.5, QColor::fromHslF(144 / 360., 0.71, 0.31, 0.5));
    pe.setColorAt(1.0, QColor::fromHslF(144 / 360., 0.71, 0.31, 0.1));
  } else {
    pe.setColorAt(0.0, QColor::fromHslF(148 / 360., 0.94, 0.51, 1.0));
    pe.setColorAt(0.5, QColor::fromHslF(112 / 360., 1.0, 0.68, 0.5));
    pe.setColorAt(1.0, QColor::fromHslF(112 / 360., 1.0, 0.68, 0.1));
  }

  painter.setBrush(pe);
  painter.drawPath(path);

  painter.restore();
}

void AnnotatedCameraWidget::drawDriverState(QPainter &painter, const UIState *s) {
  const UIScene &scene = s->scene;

  painter.save();

  // base icon
  int x = rightHandDM ? rect().right() -  (btn_size - 24) / 2 - (bdr_s * 2) : (btn_size - 24) / 2 + (bdr_s * 2);
  int y = rect().bottom() - footer_h / 2 - (conditionalExperimental ? 20 : 0);
  float opacity = dmActive ? 0.65 : 0.2;
  drawIcon(painter, x, y, dm_img, blackColor(70), opacity);

  // face
  QPointF face_kpts_draw[std::size(default_face_kpts_3d)];
  float kp;
  for (int i = 0; i < std::size(default_face_kpts_3d); ++i) {
    kp = (scene.face_kpts_draw[i].v[2] - 8) / 120 + 1.0;
    face_kpts_draw[i] = QPointF(scene.face_kpts_draw[i].v[0] * kp + x, scene.face_kpts_draw[i].v[1] * kp + y);
  }

  painter.setPen(QPen(QColor::fromRgbF(1.0, 1.0, 1.0, opacity), 5.2, Qt::SolidLine, Qt::RoundCap));
  painter.drawPolyline(face_kpts_draw, std::size(default_face_kpts_3d));

  // tracking arcs
  const int arc_l = 133;
  const float arc_t_default = 6.7;
  const float arc_t_extend = 12.0;
  QColor arc_color = QColor::fromRgbF(0.545 - 0.445 * s->engaged(),
                                      0.545 + 0.4 * s->engaged(),
                                      0.545 - 0.285 * s->engaged(),
                                      0.4 * (1.0 - dm_fade_state));
  float delta_x = -scene.driver_pose_sins[1] * arc_l / 2;
  float delta_y = -scene.driver_pose_sins[0] * arc_l / 2;
  painter.setPen(QPen(arc_color, arc_t_default+arc_t_extend*fmin(1.0, scene.driver_pose_diff[1] * 5.0), Qt::SolidLine, Qt::RoundCap));
  painter.drawArc(QRectF(std::fmin(x + delta_x, x), y - arc_l / 2, fabs(delta_x), arc_l), (scene.driver_pose_sins[1]>0 ? 90 : -90) * 16, 180 * 16);
  painter.setPen(QPen(arc_color, arc_t_default+arc_t_extend*fmin(1.0, scene.driver_pose_diff[0] * 5.0), Qt::SolidLine, Qt::RoundCap));
  painter.drawArc(QRectF(x - arc_l / 2, std::fmin(y + delta_y, y), arc_l, fabs(delta_y)), (scene.driver_pose_sins[0]>0 ? 0 : 180) * 16, 180 * 16);

  painter.restore();
}

void AnnotatedCameraWidget::drawLead(QPainter &painter, const cereal::RadarState::LeadData::Reader &lead_data, const QPointF &vd) {
  painter.save();

  const float speedBuff = frogColors ? 25. : 10.;
  const float leadBuff = frogColors ? 100. : 40.;
  const float d_rel = lead_data.getDRel();
  const float v_rel = lead_data.getVRel();

  float fillAlpha = 0;
  if (d_rel < leadBuff) {
    fillAlpha = 255 * (1.0 - (d_rel / leadBuff));
    if (v_rel < 0) {
      fillAlpha += 255 * (-1 * (v_rel / speedBuff));
    }
    fillAlpha = (int)(fmin(fillAlpha, 255));
  }

  float sz = std::clamp((25 * 30) / (d_rel / 3 + 30), 15.0f, 30.0f) * 2.35;
  float x = std::clamp((float)vd.x(), 0.f, width() - sz / 2);
  float y = std::fmin(height() - sz * .6, (float)vd.y());

  float g_xo = sz / 5;
  float g_yo = sz / 10;

  QPointF glow[] = {{x + (sz * 1.35) + g_xo, y + sz + g_yo}, {x, y - g_yo}, {x - (sz * 1.35) - g_xo, y + sz + g_yo}};
  painter.setBrush(QColor(218, 202, 37, 255));
  painter.drawPolygon(glow, std::size(glow));

  // chevron
  QPointF chevron[] = {{x + (sz * 1.25), y + sz}, {x, y}, {x - (sz * 1.25), y + sz}};
  painter.setBrush(frogColors ? frogColor(fillAlpha) : redColor(fillAlpha));
  painter.drawPolygon(chevron, std::size(chevron));

  painter.restore();
}

void AnnotatedCameraWidget::paintGL() {
  UIState *s = uiState();
  SubMaster &sm = *(s->sm);
  const double start_draw_t = millis_since_boot();
  const cereal::ModelDataV2::Reader &model = sm["modelV2"].getModelV2();
  const cereal::RadarState::Reader &radar_state = sm["radarState"].getRadarState();

  // draw camera frame
  {
    std::lock_guard lk(frame_lock);

    if (frames.empty()) {
      if (skip_frame_count > 0) {
        skip_frame_count--;
        qDebug() << "skipping frame, not ready";
        return;
      }
    } else {
      // skip drawing up to this many frames if we're
      // missing camera frames. this smooths out the
      // transitions from the narrow and wide cameras
      skip_frame_count = 5;
    }

    // Wide or narrow cam dependent on speed
    bool has_wide_cam = available_streams.count(VISION_STREAM_WIDE_ROAD) && !s->scene.wide_camera_disabled;
    if (has_wide_cam) {
      float v_ego = sm["carState"].getCarState().getVEgo();
      if ((v_ego < 10) || available_streams.size() == 1) {
        wide_cam_requested = true;
      } else if (v_ego > 15) {
        wide_cam_requested = false;
      }
      wide_cam_requested = wide_cam_requested && sm["controlsState"].getControlsState().getExperimentalMode();
      // for replay of old routes, never go to widecam
      wide_cam_requested = wide_cam_requested && s->scene.calibration_wide_valid;
    }
    CameraWidget::setStreamType(wide_cam_requested ? VISION_STREAM_WIDE_ROAD : VISION_STREAM_ROAD);

    s->scene.wide_cam = CameraWidget::getStreamType() == VISION_STREAM_WIDE_ROAD;
    if (s->scene.calibration_valid) {
      auto calib = s->scene.wide_cam ? s->scene.view_from_wide_calib : s->scene.view_from_calib;
      CameraWidget::updateCalibration(calib);
    } else {
      CameraWidget::updateCalibration(DEFAULT_CALIBRATION);
    }
    CameraWidget::setFrameId(model.getFrameId());
    CameraWidget::paintGL();
  }

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setPen(Qt::NoPen);

  if (s->worldObjectsVisible()) {
    if (sm.rcv_frame("modelV2") > s->scene.started_frame) {
      update_model(s, sm["modelV2"].getModelV2(), sm["uiPlan"].getUiPlan());
      if (sm.rcv_frame("radarState") > s->scene.started_frame) {
        update_leads(s, radar_state, sm["modelV2"].getModelV2().getPosition());
      }
    }

    drawLaneLines(painter, s);

    if (s->scene.longitudinal_control) {
      auto lead_one = radar_state.getLeadOne();
      auto lead_two = radar_state.getLeadTwo();
      if (lead_one.getStatus()) {
        drawLead(painter, lead_one, s->scene.lead_vertices[0]);
      }
      if (lead_two.getStatus() && (std::abs(lead_one.getDRel() - lead_two.getDRel()) > 3.0)) {
        drawLead(painter, lead_two, s->scene.lead_vertices[1]);
      }
    }
  }

  // DMoji - Hide the icon when the turn signal animation is on
  if (!hideDM && (sm.rcv_frame("driverStateV2") > s->scene.started_frame) && !muteDM && (!frogSignals || (frogSignals && !turnSignalLeft && !turnSignalRight))) {
    update_dmonitoring(s, sm["driverStateV2"].getDriverStateV2(), dm_fade_state, rightHandDM);
    drawDriverState(painter, s);
  }

  drawHud(painter);

  double cur_draw_t = millis_since_boot();
  double dt = cur_draw_t - prev_draw_t;
  double fps = fps_filter.update(1. / dt * 1000);
  if (fps < 15) {
    LOGW("slow frame rate: %.2f fps", fps);
  }
  prev_draw_t = cur_draw_t;

  // publish debug msg
  MessageBuilder msg;
  auto m = msg.initEvent().initUiDebug();
  m.setDrawTimeMillis(cur_draw_t - start_draw_t);
  pm->send("uiDebug", msg);
}

void AnnotatedCameraWidget::showEvent(QShowEvent *event) {
  CameraWidget::showEvent(event);

  ui_update_params(uiState());
  prev_draw_t = millis_since_boot();
}

// FrogPilot widgets

void AnnotatedCameraWidget::drawAdjustableFollowDistance(QPainter &p) {
  // Declare the variables
  static QElapsedTimer timer;
  static bool displayText = true;
  static int lastProfile = -1;
  constexpr int fadeDuration = 1000; // 1 second
  constexpr int textDuration = 3000; // 3 seconds
  
  // Enable Antialiasing
  p.setRenderHint(QPainter::Antialiasing);

  // Set the x and y coordinates
  int x = rightHandDM ? rect().right() - (btn_size - 24) / 2 - (bdr_s * 2) - (muteDM ? 50 : 250) : (btn_size - 24) / 2 + (bdr_s * 2) + (muteDM ? 50 : 250);
  const int y = rect().bottom() - (conditionalExperimental ? 20 : 0) - footer_h / 2;

  // Select the appropriate profile image/text
  int index = qBound(0, adjustableFollowDistanceProfile - 1, 2);
  QPixmap& profile_image = profile_data[index].first;
  QString profile_text = profile_data[index].second;

  // Set "displayText" to "true" when the user changes profiles
  if (lastProfile != adjustableFollowDistanceProfile) {
    displayText = true;
    lastProfile = adjustableFollowDistanceProfile;
    timer.restart();
  }

  // Set the text display
  displayText = !timer.hasExpired(textDuration);

  // Set the elapsed time since the profile switch
  int elapsed = timer.elapsed();

  // Calculate the opacity for the text and image based on the elapsed time
  qreal textOpacity = qBound(0.0, (1.0 - static_cast<qreal>(elapsed - textDuration) / fadeDuration), 1.0);
  qreal imageOpacity = qBound(0.0, (static_cast<qreal>(elapsed - textDuration) / fadeDuration), 1.0);

  // Draw the profile text with the calculated opacity
  if (textOpacity > 0.0) {
    configFont(p, "Inter", 50, "Bold");
    p.setPen(QColor(255, 255, 255));
    // Calculate the center position for text
    QFontMetrics fontMetrics(p.font());
    int textWidth = fontMetrics.horizontalAdvance(profile_text);
    // Apply opacity to the text
    p.setOpacity(textOpacity);
    p.drawText(x - textWidth / 2, y + fontMetrics.height() / 2, profile_text);
  }

  // Draw the profile image with the calculated opacity
  if (imageOpacity > 0.0) {
    drawIcon(p, x, y, profile_image, blackColor(0), imageOpacity);
  }
}

void AnnotatedCameraWidget::drawCompass(QPainter &p) {
  // Variable declarations
  const QBrush bg = blackColor(100);
  constexpr int circle_size = 250;
  constexpr int circle_offset = circle_size / 2;
  constexpr int degreeLabelOffset = circle_offset + 25;
  constexpr int inner_compass = btn_size / 2;
  int x = !rightHandDM ? rect().right() - btn_size / 2 - (bdr_s * 2) - 10 : btn_size / 2 + (bdr_s * 2) + 10;
  const int y = rect().bottom() - 20 - (conditionalExperimental ? 60 : 0) - footer_h / 2;

  // Enable Antialiasing
  p.setRenderHint(QPainter::Antialiasing);

  // Configure the circles
  p.setPen(QPen(Qt::white, 2));
  const auto drawCircle = [&](const int offset, const QBrush brush = Qt::NoBrush) {
    p.setOpacity(1.0);
    p.setBrush(brush);
    p.drawEllipse(x - offset, y - offset, offset * 2, offset * 2);
  };

  // Draw the circle background and white inner circle
  drawCircle(circle_offset, bg);

  // Rotate and draw the compass_inner_img image
  p.save();
  p.translate(x, y);
  p.rotate(bearingDeg);
  p.drawPixmap(-compass_inner_img.width() / 2, -compass_inner_img.height() / 2, compass_inner_img);
  p.restore();

  // Draw the cardinal directions
  configFont(p, "Inter", 25, "Bold");
  const auto drawDirection = [&](const QString &text, const int from, const int to, const int align) {
    // Move the "E" and "W" directions a bit closer to the middle so its more uniform
    const int offset = (text == "E") ? -5 : ((text == "W") ? 5 : 0);
    // Set the opacity based on whether the direction label is currently being pointed at
    p.setOpacity((bearingDeg >= from && bearingDeg < to) ? 1.0 : 0.2);
    p.drawText(QRect(x - inner_compass + offset, y - inner_compass, btn_size, btn_size), align, text);
  };
  drawDirection("N", 0, 68, Qt::AlignTop | Qt::AlignHCenter);
  drawDirection("E", 23, 158, Qt::AlignRight | Qt::AlignVCenter);
  drawDirection("S", 113, 248, Qt::AlignBottom | Qt::AlignHCenter);
  drawDirection("W", 203, 338, Qt::AlignLeft | Qt::AlignVCenter);
  drawDirection("N", 293, 360, Qt::AlignTop | Qt::AlignHCenter);

  // Draw the white circle outlining the cardinal directions
  drawCircle(inner_compass + 5);

  // Draw the white circle outlining the bearing degrees
  drawCircle(degreeLabelOffset);

  // Draw the black background for the bearing degrees
  QPainterPath outerCircle, innerCircle;
  outerCircle.addEllipse(x - degreeLabelOffset, y - degreeLabelOffset, degreeLabelOffset * 2, degreeLabelOffset * 2);
  innerCircle.addEllipse(x - circle_offset, y - circle_offset, circle_size, circle_size);
  p.setOpacity(1.0);
  p.fillPath(outerCircle.subtracted(innerCircle), Qt::black);

  // Draw degree lines and bearing degrees
  const auto drawCompassElements = [&](const int angle) {
    const bool isCardinalDirection = angle % 90 == 0;
    const int lineLength = isCardinalDirection ? 15 : 10;
    const int lineWidth = isCardinalDirection ? 3 : 1;
    bool isBold = abs(angle - static_cast<int>(bearingDeg)) <= 7;

    // Set the current bearing degree value to bold
    p.setFont(QFont("Inter", 8, isBold ? QFont::Bold : QFont::Normal));
    p.setPen(QPen(Qt::white, lineWidth));

    // Place the elements in their respective spots around their circles
    p.save();
    p.translate(x, y);
    p.rotate(angle);
    p.drawLine(0, -(circle_size / 2 - lineLength), 0, -(circle_size / 2));
    p.translate(0, -(circle_size / 2 + 12));
    p.rotate(-angle);
    p.drawText(QRect(-20, -10, 40, 20), Qt::AlignCenter, QString::number(angle));
    p.restore();
  };

  for (int i = 0; i < 360; i += 15) {
    drawCompassElements(i);
  }
}

void AnnotatedCameraWidget::drawConditionalExperimentalStatus(QPainter &p) {
  p.setOpacity(1.0);
  QRect statusBarRect(rect().left(), rect().bottom() - 59, rect().width(), 60);
  p.fillRect(statusBarRect, QColor(0, 0, 0, 150));

  QString statusText = !is_cruise_set ? "Conditional Experimental Mode ready" :
                      (conditionalOverridden == 1 ? "Conditional Experimental Mode overridden. Double press the \"LKAS\" button to revert" :
                       conditionalOverridden == 2 ? "Experimental Mode manually activated. Double press the \"LKAS\" button to revert" :
                       conditionalStatus == 1 ? "Conditional Experimental Mode overridden. Double tap the screen to revert" :
                       conditionalStatus == 2 ? "Experimental Mode manually activated. Double tap the screen to revert" :
                       conditionalStatus == 3 ? "Experimental Mode activated for turn / lane change" :
                       conditionalStatus == 4 ? "Experimental Mode activated for stop sign / stop light" :
                       conditionalStatus == 5 ? "Experimental Mode activated for curve" :
                       conditionalStatus == 6 ? "Experimental Mode activated due to speed being less than " + QString::number(conditionalSpeed) + " mph" :
                       conditionalStatus == 7 ? "Experimental Mode activated due to speed being less than " + QString::number(conditionalSpeedLead) + " mph" :
                       "Conditional Experimental Mode ready");

  configFont(p, "Inter", 40, "Bold");
  QRect textRect = p.fontMetrics().boundingRect(statusText);
  textRect.moveCenter(statusBarRect.center());
  p.setPen(Qt::white);
  p.drawText(textRect, Qt::AlignCenter, statusText);
}

void AnnotatedCameraWidget::drawFrogSignals(QPainter &p) {
  // Declare the turn signal size
  constexpr int signalHeight = 480;
  constexpr int signalWidth = 360;

  // Calculate the vertical position for the turn signals
  const int baseYPosition = (height() - signalHeight) / 2 + (conditionalExperimental ? 225 : 300);
  // Calculate the x-coordinates for the turn signals
  int leftSignalXPosition = width() + 75 - signalWidth - (!blindspotLeft) * 300 * animationFrameIndex;
  int rightSignalXPosition = (-75) + (!blindspotRight) * 300 * animationFrameIndex;

  // Enable Antialiasing
  p.setRenderHint(QPainter::Antialiasing);

  // Draw the turn signals
  if (animationFrameIndex < static_cast<int>(signalImgVector.size())) {
    const auto drawSignal = [&](const bool signalActivated, const int xPosition, const int flip, const int blindspot) {
      if (signalActivated) {
        // Get the appropriate image from the signalImgVector
        const QPixmap& signal = signalImgVector[animationFrameIndex + blindspot * totalFrames].transformed(QTransform().scale(flip ? -1 : 1, 1));
        // Draw the image
        p.drawPixmap(xPosition, baseYPosition, signalWidth, signalHeight, signal);
      }
    };

    // Display the animation based on which signal is activated
    drawSignal(turnSignalLeft, leftSignalXPosition, false, blindspotLeft);
    drawSignal(turnSignalRight, rightSignalXPosition, true, blindspotRight);
  }
}

void AnnotatedCameraWidget::drawRotatingWheel(QPainter &p, int x, int y) {
  // Custom steering wheel icon
  engage_img = wheel_images[steeringWheel];

  // Enable Antialiasing
  p.setRenderHint(QPainter::Antialiasing);
  
  // Set the icon according to the current status of "Experimental Mode"
  QPixmap img = steeringWheel ? engage_img : (experimentalMode ? experimental_img : engage_img);

  // Draw the icon and rotate it alongside the steering wheel
  p.setOpacity(1.0);
  p.setPen(Qt::NoPen);
  p.setBrush(conditionalStatus || conditionalOverridden ? QColor(255, 246, 0, 255) : steeringWheel && experimentalMode ? QColor(218, 111, 37, 241) : QColor(0, 0, 0, 166));
  p.drawEllipse(x - btn_size / 2, y - btn_size / 2, btn_size, btn_size);
  p.save();
  p.translate(x, y);
  p.rotate(-steeringAngleDeg);
  p.drawPixmap(-img.size().width() / 2, -img.size().height() / 2, img);
  p.restore();
}
