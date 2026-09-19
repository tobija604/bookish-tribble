#pragma once
// Tiny screen-navigation helper on top of LVGL: builds a consistent app bar
// (title + back button) and tracks a one-level "back" target, since the
// touch menu (spec section 4) is a flat list of 12 destinations from HOME,
// not a deep navigation stack.
#include <lvgl.h>
#include <Arduino.h>
#include <functional>
#include <map>

class ScreenManager {
 public:
  static ScreenManager& instance() {
    static ScreenManager m;
    return m;
  }

  using BackFn = std::function<void()>;

  // Creates a full-screen container with a title bar; `onBack` is called
  // when the back button is tapped (usually: go to Home or MainMenu).
  lv_obj_t* createScreen(const char* title, BackFn onBack) {
    lv_obj_t* scr = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x0B0F19), 0);

    lv_obj_t* bar = lv_obj_create(scr);
    lv_obj_set_size(bar, LV_PCT(100), 40);
    lv_obj_align(bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(bar, 0, 0);

    lv_obj_t* label = lv_label_create(bar);
    lv_label_set_text(label, title);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);

    if (onBack) {
      lv_obj_t* backBtn = lv_btn_create(bar);
      lv_obj_set_size(backBtn, 60, 32);
      lv_obj_align(backBtn, LV_ALIGN_LEFT_MID, 4, 0);
      lv_obj_t* backLabel = lv_label_create(backBtn);
      lv_label_set_text(backLabel, LV_SYMBOL_LEFT);
      static BackFn storedFn; // simple approach: one back handler active at a time (flat nav model)
      storedFn = onBack;
      lv_obj_add_event_cb(backBtn, [](lv_event_t* e) {
        (void)e; storedFn();
      }, LV_EVENT_CLICKED, nullptr);
    }

    lv_obj_t* content = lv_obj_create(scr);
    lv_obj_set_size(content, LV_PCT(100), LV_PCT(100) - 40);
    lv_obj_align(content, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    contentByScreen_[scr] = content;

    return scr;
  }

  lv_obj_t* contentOf(lv_obj_t* screen) { return contentByScreen_[screen]; }

  void show(lv_obj_t* screen) { lv_scr_load(screen); }

 private:
  ScreenManager() = default;
  std::map<lv_obj_t*, lv_obj_t*> contentByScreen_;
};
