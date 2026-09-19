#pragma once
// RFID menu entry (spec sections 4 & 6). On-device this covers the "SCAN
// CARD" capture step of ADD CARD (uid capture only — the web UI's RFID >
// CARDS screen is where a captured card actually gets assigned to a user,
// enabled/disabled/edited/deleted/tested, per spec 6's full button set,
// consistent with the touch-vs-web division of labor noted in InfoScreen.h).
// A card added here is created unassigned and stays denied on scan until
// an admin assigns a user to it on the web dashboard.
#include <lvgl.h>
#include <functional>
#include "ScreenManager.h"
#include "../../rfid/RfidManager.h"
#include "../../rfid/CardStore.h"
#include "../../locale/Locale.h"

class RfidScreen {
 public:
  static lv_obj_t* build(std::function<void()> onBack) {
    lv_obj_t* scr = ScreenManager::instance().createScreen(Locale::instance().t("menu.rfid").c_str(), onBack);
    lv_obj_t* content = ScreenManager::instance().contentOf(scr);

    lv_obj_t* count = lv_label_create(content);
    lv_obj_set_style_text_color(count, lv_color_white(), 0);
    lv_obj_align(count, LV_ALIGN_TOP_MID, 0, 10);
    char buf[32];
    snprintf(buf, sizeof(buf), "%d kartic v evidenci", (int)CardStore::instance().all().size());
    lv_label_set_text(count, buf);

    lv_obj_t* status = lv_label_create(content);
    lv_obj_set_style_text_color(status, lv_color_hex(0x8890A3), 0);
    lv_obj_align(status, LV_ALIGN_CENTER, 0, 0);
    lv_label_set_text(status, "");

    lv_obj_t* btn = lv_btn_create(content);
    lv_obj_set_size(btn, 160, 50);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_t* lbl = lv_label_create(btn);
    lv_label_set_text(lbl, "DODAJ KARTICO");
    lv_obj_center(lbl);
    lv_obj_set_user_data(btn, status);
    lv_obj_add_event_cb(btn, [](lv_event_t* e) {
      lv_obj_t* statusLbl = (lv_obj_t*)lv_obj_get_user_data(lv_event_get_target(e));
      lv_label_set_text(statusLbl, Locale::instance().t("scan.card").c_str());
      RfidManager::instance().captureNextUid([statusLbl](const String& uid) {
        if (CardStore::instance().exists(uid)) {
          lv_label_set_text(statusLbl, ("Kartica je že znana: " + uid).c_str());
          return;
        }
        CardStore::instance().add(uid, "" /*unassigned*/, "");
        lv_label_set_text(statusLbl, ("KARTICA DODANA\n" + uid + "\n(dodelite uporabnika na spletni nadzorni plošči)").c_str());
      });
    }, LV_EVENT_CLICKED, nullptr);

    return scr;
  }
};
