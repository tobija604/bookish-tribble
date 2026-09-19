#pragma once
// Touch-initiated unlock (spec section 56): touch alone must NOT unlock the
// box. This screen arms a one-shot RFID capture and offers a security-code
// keypad; either a valid card or a correct/valid code triggers the same
// RequestUnlock path used by RFID and the web UI.
#include <lvgl.h>
#include <functional>
#include "ScreenManager.h"
#include "PinKeypad.h"
#include "../../rfid/RfidManager.h"
#include "../../rfid/CardStore.h"
#include "../../users/UserManager.h"
#include "../../security/SecurityManager.h"
#include "../../core/EventBus.h"
#include "../../locale/Locale.h"

class UnlockScreen {
 public:
  static lv_obj_t* build(std::function<void()> onBack) {
    lv_obj_t* scr = ScreenManager::instance().createScreen(Locale::instance().t("menu.unlock").c_str(), onBack);
    lv_obj_t* content = ScreenManager::instance().contentOf(scr);

    lv_obj_t* prompt = lv_label_create(content);
    lv_label_set_long_mode(prompt, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(prompt, LV_PCT(90));
    lv_obj_set_style_text_color(prompt, lv_color_white(), 0);
    lv_obj_align(prompt, LV_ALIGN_TOP_MID, 0, 4);
    lv_label_set_text(prompt, "Približajte RFID kartico ali vnesite varnostno kodo, prikazano na spletni nadzorni plošči.");

    lv_obj_t* keypadHolder = lv_obj_create(content);
    lv_obj_set_size(keypadHolder, LV_PCT(100), LV_PCT(80));
    lv_obj_align(keypadHolder, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(keypadHolder, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(keypadHolder, 0, 0);

    PinKeypad::build(keypadHolder, 6, [](const String& code) {
      if (SecurityManager::instance().tryConsumeSecurityCode(code)) {
        EventBus::instance().emit(Topic::RequestUnlock);
      } else {
        // Wrong/expired code: SecurityManager::tryConsumeSecurityCode already
        // counted the failure toward the tamper threshold; just let the
        // user try again (no separate error dialog in this skeleton).
      }
    });

    // Arm a one-shot RFID capture for this screen: a valid card here goes
    // through the SAME authorization path as a normal wall-mounted scan
    // (RfidManager::checkAccess), not a bypass — captureNextUid is only
    // used for the web "ADD CARD" UID-capture flow elsewhere; here we
    // deliberately do NOT use it, so a card tap while this screen is open
    // is checked normally by RfidManager::loop() with no special casing.

    return scr;
  }
};
