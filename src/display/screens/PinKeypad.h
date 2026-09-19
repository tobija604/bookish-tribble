#pragma once
// Reusable on-screen numeric keypad (spec section 2: "PIN tipkovnico").
// Used by UnlockScreen for security-code entry and by admin-only flows
// (e.g. confirming FACTORY RESET) that need a short numeric confirmation.
#include <lvgl.h>
#include <Arduino.h>
#include <functional>

class PinKeypad {
 public:
  using SubmitFn = std::function<void(const String& code)>;

  // Builds a keypad + masked entry field inside `parent`, calling
  // `onSubmit` once `digits` characters have been entered.
  static lv_obj_t* build(lv_obj_t* parent, int digits, SubmitFn onSubmit) {
    lv_obj_t* wrap = lv_obj_create(parent);
    lv_obj_set_size(wrap, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(wrap, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(wrap, 0, 0);

    lv_obj_t* entry = lv_label_create(wrap);
    lv_obj_set_style_text_font(entry, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(entry, lv_color_white(), 0);
    lv_obj_align(entry, LV_ALIGN_TOP_MID, 0, 4);
    lv_label_set_text(entry, "");

    auto* state = new KeypadState{digits, "", onSubmit, entry};

    lv_obj_t* grid = lv_obj_create(wrap);
    lv_obj_set_size(grid, LV_PCT(90), LV_PCT(75));
    lv_obj_align(grid, LV_ALIGN_BOTTOM_MID, 0, -4);
    lv_obj_set_style_bg_opa(grid, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(grid, 0, 0);
    lv_obj_set_layout(grid, LV_LAYOUT_GRID);
    static lv_coord_t cols[] = {70, 70, 70, LV_GRID_TEMPLATE_LAST};
    static lv_coord_t rows[] = {50, 50, 50, 50, LV_GRID_TEMPLATE_LAST};
    lv_obj_set_grid_dsc_array(grid, cols, rows);

    const char* keys[12] = {"1","2","3","4","5","6","7","8","9","BRIS","0","OK"};
    for (int i = 0; i < 12; i++) {
      lv_obj_t* btn = lv_btn_create(grid);
      lv_obj_set_grid_cell(btn, LV_GRID_ALIGN_STRETCH, i % 3, 1, LV_GRID_ALIGN_STRETCH, i / 3, 1);
      lv_obj_t* lbl = lv_label_create(btn);
      lv_label_set_text(lbl, keys[i]);
      lv_obj_center(lbl);
      lv_obj_set_user_data(btn, state);
      const char* key = keys[i];
      lv_obj_add_event_cb(btn, [](lv_event_t* e) {
        auto* st = (KeypadState*)lv_obj_get_user_data(lv_event_get_target(e));
        const char* k = (const char*)lv_event_get_user_data(e);
        String ks(k);
        if (ks == "BRIS") { if (st->value.length()) st->value.remove(st->value.length() - 1); }
        else if (ks == "OK") { if (st->value.length() == st->digits) st->onSubmit(st->value); }
        else if (st->value.length() < st->digits) st->value += ks;

        String masked;
        for (uint16_t i = 0; i < st->value.length(); i++) masked += "*";
        lv_label_set_text(st->entryLabel, masked.c_str());

        if ((int)st->value.length() == st->digits) st->onSubmit(st->value);
      }, LV_EVENT_CLICKED, (void*)key);
    }

    return wrap;
  }

 private:
  struct KeypadState {
    int digits;
    String value;
    SubmitFn onSubmit;
    lv_obj_t* entryLabel;
  };
};
