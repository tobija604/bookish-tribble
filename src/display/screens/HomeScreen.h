#pragma once
// Default Home Screen (spec section 3): LOCKED/UNLOCKED, WiFi dot, clock,
// date, lid state, last access, tap-anywhere opens the main menu.
#include <lvgl.h>
#include <time.h>
#include <functional>
#include "../../core/EventBus.h"
#include "../../servo/LockController.h"
#include "../../lid/LidSensor.h"
#include "../../wifi/WifiManager.h"
#include "../../users/UserManager.h"
#include "../../events/EventLogger.h"
#include "../../locale/Locale.h"

class HomeScreen {
 public:
  static HomeScreen& instance() {
    static HomeScreen s;
    return s;
  }

  using OpenMenuFn = std::function<void()>;

  lv_obj_t* build(OpenMenuFn onOpenMenu) {
    scr_ = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(scr_, lv_color_hex(0x0B0F19), 0);
    lv_obj_add_flag(scr_, LV_OBJ_FLAG_CLICKABLE);
    onOpenMenu_ = onOpenMenu;
    lv_obj_add_event_cb(scr_, [](lv_event_t* e) {
      HomeScreen::instance().onOpenMenu_();
      (void)e;
    }, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* title = lv_label_create(scr_);
    lv_label_set_text(title, "SMART BOX");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 30);

    lockLabel_ = lv_label_create(scr_);
    lv_obj_set_style_text_font(lockLabel_, &lv_font_montserrat_28, 0);
    lv_obj_align(lockLabel_, LV_ALIGN_CENTER, 0, -60);

    lidLabel_ = lv_label_create(scr_);
    lv_obj_set_style_text_color(lidLabel_, lv_color_hex(0x8890A3), 0);
    lv_obj_align(lidLabel_, LV_ALIGN_CENTER, 0, -20);

    wifiDot_ = lv_obj_create(scr_);
    lv_obj_set_size(wifiDot_, 10, 10);
    lv_obj_set_style_radius(wifiDot_, LV_RADIUS_CIRCLE, 0);
    lv_obj_align(wifiDot_, LV_ALIGN_CENTER, -40, 10);

    clockLabel_ = lv_label_create(scr_);
    lv_obj_set_style_text_font(clockLabel_, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(clockLabel_, lv_color_white(), 0);
    lv_obj_align(clockLabel_, LV_ALIGN_CENTER, 0, 40);

    dateLabel_ = lv_label_create(scr_);
    lv_obj_set_style_text_color(dateLabel_, lv_color_hex(0x8890A3), 0);
    lv_obj_align(dateLabel_, LV_ALIGN_CENTER, 0, 70);

    lastAccessLabel_ = lv_label_create(scr_);
    lv_obj_set_style_text_color(lastAccessLabel_, lv_color_hex(0x8890A3), 0);
    lv_obj_set_style_text_font(lastAccessLabel_, &lv_font_montserrat_12, 0);
    lv_obj_align(lastAccessLabel_, LV_ALIGN_BOTTOM_MID, 0, -30);

    EventBus::instance().on(Topic::LockStateChanged, [this](const EventPayload&) { refreshLock(); });
    EventBus::instance().on(Topic::LidStateChanged, [this](const EventPayload&) { refreshLid(); });
    EventBus::instance().on(Topic::WifiConnected, [this](const EventPayload&) { refreshWifi(); });
    EventBus::instance().on(Topic::WifiDisconnected, [this](const EventPayload&) { refreshWifi(); });

    refreshLock(); refreshLid(); refreshWifi(); refreshLastAccess();
    return scr_;
  }

  // Called periodically (~1/sec) from main.cpp's UI tick, not every LVGL frame.
  void tick() {
    time_t now = time(nullptr);
    struct tm t; localtime_r(&now, &t);
    char buf[16];
    bool use24h = true; // pulled from WifiConfig::use24h in a full build
    if (use24h) snprintf(buf, sizeof(buf), "%02d:%02d", t.tm_hour, t.tm_min);
    else snprintf(buf, sizeof(buf), "%02d:%02d %s", (t.tm_hour % 12 == 0) ? 12 : t.tm_hour % 12,
                   t.tm_min, t.tm_hour < 12 ? "dop." : "pop.");
    lv_label_set_text(clockLabel_, buf);

    char dbuf[16];
    snprintf(dbuf, sizeof(dbuf), "%02d.%02d.%04d", t.tm_mday, t.tm_mon + 1, t.tm_year + 1900);
    lv_label_set_text(dateLabel_, dbuf);
  }

 private:
  HomeScreen() = default;
  lv_obj_t* scr_ = nullptr;
  lv_obj_t *lockLabel_, *lidLabel_, *wifiDot_, *clockLabel_, *dateLabel_, *lastAccessLabel_;
  OpenMenuFn onOpenMenu_;

  void refreshLock() {
    bool locked = (LockController::instance().state() == LockState::LOCKED);
    bool error = (LockController::instance().state() == LockState::ERROR);
    if (error) {
      lv_label_set_text(lockLabel_, ("⚠ " + Locale::instance().t("error.lock")).c_str());
      lv_obj_set_style_text_color(lockLabel_, lv_color_hex(0xF59E0B), 0);
    } else {
      String label = (locked ? "🔒 " : "🔓 ") + Locale::instance().t(locked ? "home.locked" : "home.unlocked");
      lv_label_set_text(lockLabel_, label.c_str());
      lv_obj_set_style_text_color(lockLabel_, lv_color_hex(locked ? 0xEF4444 : 0x22C55E), 0);
    }
  }

  void refreshLid() {
    bool open = (LidSensor::instance().state() == LidState::OPEN);
    lv_label_set_text(lidLabel_, open ? Locale::instance().t("home.open").c_str()
                                       : Locale::instance().t("home.closed").c_str());
  }

  void refreshWifi() {
    bool online = (WifiManager::instance().state() == WifiState::CONNECTED);
    lv_obj_set_style_bg_color(wifiDot_, lv_color_hex(online ? 0x22C55E : 0x8890A3), 0);
  }

  void refreshLastAccess() {
    if (EventLogger::instance().recent().empty()) {
      lv_label_set_text(lastAccessLabel_, "brez dostopa doslej");
      return;
    }
    const auto& e = EventLogger::instance().recent().back();
    String who = e.user.length() ? e.user : e.rfidUid;
    char buf[48];
    snprintf(buf, sizeof(buf), "Zadnji dostop: %s", who.length() ? who.c_str() : "-");
    lv_label_set_text(lastAccessLabel_, buf);
  }
};
