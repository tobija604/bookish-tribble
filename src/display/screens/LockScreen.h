#pragma once
// LOCK menu entry (spec section 4/15): a straightforward confirm-then-lock
// screen. Unlike UNLOCK, locking the box is not treated as a
// security-sensitive action needing RFID/code — anyone with physical access
// to the touchscreen may lock it (consistent with spec: authentication is
// only required to grant access, not to re-secure it).
#include <lvgl.h>
#include <functional>
#include "ScreenManager.h"
#include "../../core/EventBus.h"
#include "../../servo/LockController.h"
#include "../../locale/Locale.h"

class LockScreen {
 public:
  static lv_obj_t* build(std::function<void()> onBack) {
    lv_obj_t* scr = ScreenManager::instance().createScreen(Locale::instance().t("menu.lock").c_str(), onBack);
    lv_obj_t* content = ScreenManager::instance().contentOf(scr);

    lv_obj_t* status = lv_label_create(content);
    lv_obj_set_style_text_color(status, lv_color_white(), 0);
    lv_obj_align(status, LV_ALIGN_TOP_MID, 0, 20);
    lv_label_set_text(status, LockController::instance().state() == LockState::LOCKED
                                   ? "Že ZAKLENJENO" : "Trenutno ODKLENJENO");

    lv_obj_t* btn = lv_btn_create(content);
    lv_obj_set_size(btn, 160, 60);
    lv_obj_align(btn, LV_ALIGN_CENTER, 0, 20);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0xEF4444), 0);
    lv_obj_t* lbl = lv_label_create(btn);
    lv_label_set_text(lbl, "ZAKLENI ZDAJ");
    lv_obj_center(lbl);
    lv_obj_add_event_cb(btn, [](lv_event_t*) {
      EventBus::instance().emit(Topic::RequestLock);
    }, LV_EVENT_CLICKED, nullptr);

    return scr;
  }
};
