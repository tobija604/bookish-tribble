#pragma once
// Main touch menu — the 12 items from spec section 4. Admin-only extras
// (spec: "Administrator vidi dodatne funkcije") would filter this list by
// the currently-authenticated web/admin session; on-device there is no
// per-user login, so all 12 tiles are shown and any action that's
// genuinely sensitive (UNLOCK) has its own auth gate (UnlockScreen).
// Tile labels and screen titles are pulled from Locale (default: sl —
// Slovenian is the primary on-device language per spec section 49).
#include <lvgl.h>
#include <functional>
#include <vector>
#include "ScreenManager.h"
#include "InfoScreen.h"
#include "UnlockScreen.h"
#include "LockScreen.h"
#include "RfidScreen.h"
#include "../../users/UserManager.h"
#include "../../events/EventLogger.h"
#include "../../wifi/WifiManager.h"
#include "../../security/SecurityManager.h"
#include "../../diagnostics/DiagnosticsManager.h"
#include "../../power/PowerManager.h"
#include "../../lid/LidSensor.h"
#include "../../servo/LockController.h"
#include "../../rfid/RfidManager.h"
#include "../../config/ConfigManager.h"
#include "../../locale/Locale.h"

class MainMenu {
 public:
  static lv_obj_t* build(std::function<void()> onHome) {
    lv_obj_t* scr = ScreenManager::instance().createScreen(Locale::instance().t("menu.title").c_str(), onHome);
    lv_obj_t* content = ScreenManager::instance().contentOf(scr);

    lv_obj_set_layout(content, LV_LAYOUT_GRID);
    static lv_coord_t cols[] = {140, 140, 140, LV_GRID_TEMPLATE_LAST};
    static lv_coord_t rows[] = {70, 70, 70, 70, LV_GRID_TEMPLATE_LAST};
    lv_obj_set_grid_dsc_array(content, cols, rows);

    auto& L = Locale::instance();
    struct Item { String label; std::function<void()> action; };
    std::vector<Item> items = {
      {L.t("menu.status"), [onHome]() { ScreenManager::instance().show(
          InfoScreen::build(Locale::instance().t("menu.status").c_str(), statusText, [onHome]() { showMenu(onHome); })); }},
      {L.t("menu.unlock"), [onHome]() { ScreenManager::instance().show(UnlockScreen::build([onHome]() { showMenu(onHome); })); }},
      {L.t("menu.lock"), [onHome]() { ScreenManager::instance().show(LockScreen::build([onHome]() { showMenu(onHome); })); }},
      {L.t("menu.rfid"), [onHome]() { ScreenManager::instance().show(RfidScreen::build([onHome]() { showMenu(onHome); })); }},
      {L.t("menu.users"), [onHome]() { ScreenManager::instance().show(
          InfoScreen::build(Locale::instance().t("menu.users").c_str(), usersText, [onHome]() { showMenu(onHome); })); }},
      {L.t("menu.events"), [onHome]() { ScreenManager::instance().show(
          InfoScreen::build(Locale::instance().t("menu.events").c_str(), eventsText, [onHome]() { showMenu(onHome); })); }},
      {L.t("menu.settings"), [onHome]() { ScreenManager::instance().show(
          InfoScreen::build(Locale::instance().t("menu.settings").c_str(), settingsText, [onHome]() { showMenu(onHome); })); }},
      {L.t("menu.network"), [onHome]() { ScreenManager::instance().show(
          InfoScreen::build(Locale::instance().t("menu.network").c_str(), networkText, [onHome]() { showMenu(onHome); })); }},
      {L.t("menu.security"), [onHome]() { ScreenManager::instance().show(
          InfoScreen::build(Locale::instance().t("menu.security").c_str(), securityText, [onHome]() { showMenu(onHome); })); }},
      {L.t("menu.device"), [onHome]() { ScreenManager::instance().show(
          InfoScreen::build(Locale::instance().t("menu.device").c_str(), deviceText, [onHome]() { showMenu(onHome); })); }},
      {L.t("menu.diagnostics"), [onHome]() { ScreenManager::instance().show(
          InfoScreen::build(Locale::instance().t("menu.diagnostics").c_str(), diagnosticsText, [onHome]() { showMenu(onHome); })); }},
      {L.t("menu.about"), [onHome]() { ScreenManager::instance().show(
          InfoScreen::build(Locale::instance().t("menu.about").c_str(), aboutText, [onHome]() { showMenu(onHome); })); }},
    };

    for (auto& item : items) {
      lv_obj_t* btn = lv_btn_create(content);
      lv_obj_t* lbl = lv_label_create(btn);
      lv_label_set_text(lbl, item.label.c_str());
      lv_obj_center(lbl);
      auto* action = new std::function<void()>(item.action);
      lv_obj_set_user_data(btn, action);
      lv_obj_add_event_cb(btn, [](lv_event_t* e) {
        auto* fn = (std::function<void()>*)lv_obj_get_user_data(lv_event_get_target(e));
        (*fn)();
      }, LV_EVENT_CLICKED, nullptr);
    }
    // Grid cell placement done via LVGL's automatic flow when no explicit
    // cell is set is NOT default in v8 — assign cells explicitly:
    for (size_t i = 0; i < lv_obj_get_child_cnt(content) && i < items.size(); i++) {
      lv_obj_t* btn = lv_obj_get_child(content, i);
      lv_obj_set_grid_cell(btn, LV_GRID_ALIGN_STRETCH, i % 3, 1, LV_GRID_ALIGN_STRETCH, i / 3, 1);
    }

    return scr;
  }

 private:
  static void showMenu(std::function<void()> onHome) {
    ScreenManager::instance().show(build(onHome));
  }

  static String lockStateLabelSl() {
    switch (LockController::instance().state()) {
      case LockState::LOCKED: return "ZAKLENJENO";
      case LockState::UNLOCKED: return "ODKLENJENO";
      case LockState::MOVING: return "V PREMIKU";
      case LockState::ERROR: return "NAPAKA";
      default: return "NEZNANO";
    }
  }

  static String statusText() {
    String s;
    s += String("Ključavnica: ") + lockStateLabelSl() + "\n";
    s += String("Pokrov: ") + (LidSensor::instance().state() == LidState::CLOSED ? "ZAPRTO" :
                             LidSensor::instance().state() == LidState::OPEN ? "ODPRTO" : "NEZNANO") + "\n";
    s += String("WiFi: ") + (WifiManager::instance().state() == WifiState::CONNECTED ? "POVEZAN" : "BREZ POVEZAVE") + "\n";
    s += "IP: " + WifiManager::instance().ipAddress() + "\n";
    if (PowerManager::instance().isBatteryPresent()) {
      s += "Baterija: " + String(PowerManager::instance().batteryPercent()) + "%\n";
    }
    s += "Čas delovanja: " + String(millis() / 1000) + " s";
    return s;
  }

  static String usersText() {
    String s = String(UserManager::instance().all().size()) + " uporabnik(ov):\n\n";
    for (auto& u : UserManager::instance().all()) {
      s += "- " + u.username + " (" + roleToStr(u.role) + (u.active ? "" : ", onemogočen") + ")\n";
    }
    s += "\nUrejanje na spletni nadzorni plošči: UPORABNIKI.";
    return s;
  }

  static String eventsText() {
    String s = "Zadnji dogodki:\n\n";
    auto& recent = EventLogger::instance().recent();
    int shown = 0;
    for (auto it = recent.rbegin(); it != recent.rend() && shown < 8; ++it, ++shown) {
      s += String(eventTypeToStr(it->type)) + " (" + eventSourceToStr(it->source) + ")\n";
    }
    if (recent.empty()) s += "(še ni dogodkov)";
    return s;
  }

  static String settingsText() {
    return "Nastavitve naprave, servo motorjev, pokrova, zvoka, izgleda in\n"
           "avtomatizacije se urejajo na spletni nadzorni plošči: NASTAVITVE /\n"
           "SERVO MOTORJI / SENZOR POKROVA / ZVOK / IZGLED / AVTOMATIZACIJA.";
  }

  static String networkText() {
    String s;
    s += "Način: " + String(WifiManager::instance().state() == WifiState::AP_MODE ? "NASTAVITVENA TOČKA (AP)" :
                            WifiManager::instance().state() == WifiState::CONNECTED ? "POVEZAN (STA)" : "BREZ POVEZAVE") + "\n";
    s += "SSID/AP: " + (WifiManager::instance().state() == WifiState::AP_MODE
                             ? WifiManager::instance().apSsid()
                             : ConfigManager::instance().get().wifi.ssid) + "\n";
    s += "IP: " + WifiManager::instance().ipAddress() + "\n";
    s += "MAC: " + WifiManager::instance().macAddress();
    return s;
  }

  static String securityText() {
    return "Časovna omejitev spletne seje, varnostna koda in omejevanje\n"
           "poskusov se urejajo na spletni nadzorni plošči: VARNOST. Za\n"
           "generiranje enkratne varnostne kode držite GUMB 1, ko spletna\n"
           "seja prikazuje WEB ZAKLENJEN.";
  }

  static String deviceText() {
    return String("Model: ESP32-S3-Touch-AMOLED-1.75-B\nMCU: ESP32-S3R8\nFlash: 16 MB\nPSRAM: 8 MB\n") +
           "Firmware: " + SMARTBOX_FW_VERSION;
  }

  static String diagnosticsText() {
    String s;
    for (auto& r : DiagnosticsManager::instance().runAll()) {
      const char* tag = r.status == DiagStatus::OK ? "V REDU" : r.status == DiagStatus::WARN ? "OPOZORILO" :
                         r.status == DiagStatus::FAIL ? "NAPAKA" : "-";
      s += String(tag) + "  " + r.component + (r.detail.length() ? (": " + r.detail) : "") + "\n";
    }
    return s;
  }

  static String aboutText() {
    return String("SMART BOX\nModel: ESP32-S3-Touch-AMOLED-1.75-B\nFirmware: ") + SMARTBOX_FW_VERSION +
           "\nGradnja (build): " + SMARTBOX_BUILD_TIME;
  }
};
