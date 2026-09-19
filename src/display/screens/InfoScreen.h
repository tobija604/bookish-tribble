#pragma once
// Generic read-only info screen, reused for the menu entries whose primary
// editing surface is the web dashboard (STATUS, USERS, EVENTS, SETTINGS,
// NETWORK, SECURITY, DEVICE, DIAGNOSTICS, ABOUT — spec section 4, items
// 1/5/6/7/8/9/10/11/12). The touchscreen's job for these is fast glanceable
// status, not full CRUD on a 466x466 panel with a soft keyboard — deep
// editing (add/edit users, rewrite automation rules, calibrate servos...)
// is the web UI's job, consistent with spec section 25's much larger web
// menu vs. the AMOLED's 12-item flat menu. This is a deliberate design
// choice, documented in docs/UX_NOTES.md.
#include <lvgl.h>
#include <Arduino.h>
#include <functional>
#include <vector>
#include "ScreenManager.h"

class InfoScreen {
 public:
  using ContentProvider = std::function<String()>;
  using BackFn = std::function<void()>;

  static lv_obj_t* build(const char* title, ContentProvider provider, BackFn onBack) {
    lv_obj_t* scr = ScreenManager::instance().createScreen(title, onBack);
    lv_obj_t* content = ScreenManager::instance().contentOf(scr);

    lv_obj_t* label = lv_label_create(content);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(label, LV_PCT(90));
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 8);
    lv_label_set_text(label, provider().c_str());

    // Store the provider + label so main.cpp's UI tick can refresh it
    // periodically without rebuilding the whole screen.
    lv_obj_set_user_data(label, new ContentProvider(provider));
    registered().push_back(label);
    return scr;
  }

  // Called periodically from main.cpp's UI tick to refresh whichever
  // InfoScreen label is currently visible (cheap: only relabels text).
  static void refreshAll() {
    for (auto* label : registered()) {
      auto* provider = (ContentProvider*)lv_obj_get_user_data(label);
      if (provider) lv_label_set_text(label, (*provider)().c_str());
    }
  }

 private:
  static std::vector<lv_obj_t*>& registered() {
    static std::vector<lv_obj_t*> v;
    return v;
  }
};
