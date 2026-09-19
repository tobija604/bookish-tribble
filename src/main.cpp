// =============================================================================
// SMART BOX firmware — main application entry point.
//
// Boot sequence follows spec section 52 (normal boot) / 51 (first boot):
//   BOOT -> INITIALIZING -> CHECK LOCK -> CHECK LID -> CHECK RFID ->
//   CHECK SERVOS -> CHECK NETWORK -> HOME
// Every module implements IModule (core/Module.h) and is driven from one
// flat, non-blocking loop() dispatch list (spec 59) — nothing here calls
// delay() in the hot path; hardware bring-up during begin() is the only
// place blocking calls are acceptable (it runs once, before HOME is shown).
// =============================================================================
#include <Arduino.h>
#include <Wire.h>
#include <cstring>
#include "core/EventBus.h"
#include "config/ConfigManager.h"
#include "config/HardwareConfig.h"
#include "storage/StorageManager.h"
#include "events/EventLogger.h"
#include "users/UserManager.h"
#include "rfid/CardStore.h"
#include "rfid/RfidManager.h"
#include "servo/LockController.h"
#include "lid/LidSensor.h"
#include "buttons/ButtonManager.h"
#include "buttons/ButtonActionDispatcher.h"
#include "wifi/WifiManager.h"
#include "security/SecurityManager.h"
#include "automation/RuleEngine.h"
#include "audio/AudioManager.h"
#include "rtc/RtcManager.h"
#include "imu/ImuManager.h"
#include "power/PowerManager.h"
#include "sd/SdManager.h"
#include "ota/OtaManager.h"
#include "web/WebServer.h"
#include "display/DisplayManager.h"
#include "display/screens/HomeScreen.h"
#include "display/screens/MainMenu.h"
#include "display/screens/InfoScreen.h"
#include "include/pins.h"

static void goHome();
static void openMenu();
static void wireFactoryResetConfirmDialog();

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n=== SMART BOX booting ===");

  // ---- CHECK HARDWARE (spec 51/52) ----
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 400000);
  HardwareConfig::instance().begin();
  StorageManager::instance().begin();

  ConfigManager::instance().begin();
  EventLogger::instance().begin();
  UserManager::instance().begin();
  CardStore::instance().begin();

  RtcManager::instance().begin();   // time available even fully offline (spec 21)
  ImuManager::instance().begin();
  PowerManager::instance().begin();
  SdManager::instance().begin();    // optional; never blocks boot if absent (spec 37)

  // ---- CHECK LOCK / CHECK LID (spec 52) ----
  LockController::instance().begin();  // boot-syncs both servos to LOCKED, fail-safe (spec 61)
  LidSensor::instance().begin();

  // ---- CHECK RFID / CHECK SERVOS already covered above ----
  RfidManager::instance().begin();

  ButtonManager::instance().begin();
  ButtonActionDispatcher::instance().begin();
  SecurityManager::instance().begin();
  RuleEngine::instance().begin();
  AudioManager::instance().begin();
  OtaManager::instance().begin();

  // ---- CHECK NETWORK (spec 52) ----
  WifiManager::instance().begin();   // AP-mode WIFI SETUP if unprovisioned, else STA connect; never blocks (spec 22-23)

  SmartBoxWebServer::instance().begin();

  // ---- Display last: HOME is the terminal state of the boot sequence ----
  DisplayManager::instance().begin();
  goHome();
  wireFactoryResetConfirmDialog();

  EventLogger::instance().log(EventType::BOOT, EventSource::SYSTEM);
  EventBus::instance().emit(Topic::BootComplete);
  Serial.println("=== SMART BOX ready (HOME) ===");
}

void loop() {
  // Flat, non-blocking dispatch (spec 59) — order matters only in that
  // DisplayManager's LVGL pump should run often for responsive touch, so
  // it's serviced every iteration like everything else; nothing below
  // blocks for more than a few hundred microseconds per call.
  RfidManager::instance().loop();
  LockController::instance().loop();
  LidSensor::instance().loop();
  ButtonManager::instance().loop();
  WifiManager::instance().loop();
  SecurityManager::instance().loop();
  RuleEngine::instance().loop();
  AudioManager::instance().loop();
  ImuManager::instance().loop();
  PowerManager::instance().loop();
  OtaManager::instance().loop();
  DisplayManager::instance().loop();

  static uint32_t lastUiTickMs = 0;
  if (millis() - lastUiTickMs > 1000) {
    lastUiTickMs = millis();
    HomeScreen::instance().tick();
    // Cheap: only meaningfully visible while an InfoScreen is on-screen,
    // a no-op label relabel otherwise.
    InfoScreen::refreshAll();
  }
}

// ---------------------------------------------------------------------------
// Display navigation wiring (kept out of DisplayManager.h to avoid that
// module depending on every other manager's headers).
// ---------------------------------------------------------------------------
static void goHome() {
  lv_obj_t* home = HomeScreen::instance().build(openMenu);
  ScreenManager::instance().show(home);
}

static void openMenu() {
  lv_obj_t* menu = MainMenu::build(goHome);
  ScreenManager::instance().show(menu);
}

// FACTORY RESET confirmation (spec 11): the 10s BUTTON2 hold only arms the
// request; this still requires an explicit AMOLED confirmation dialog
// before anything is wiped. A parallel path exists via the web API
// (/api/system/factory-reset with confirm:true) for admins who prefer not
// to touch the device physically.
static void factoryResetMsgboxCb(lv_event_t* e) {
  lv_obj_t* mbox = lv_event_get_target(e);
  const char* txt = lv_msgbox_get_active_btn_text(mbox);
  if (txt && strcmp(txt, "IZBRIŠI VSE") == 0) {
    EventBus::instance().emit(Topic::FactoryResetConfirmed);
  }
  lv_msgbox_close(mbox);
}

static void wireFactoryResetConfirmDialog() {
  EventBus::instance().on(Topic::FactoryResetRequested, [](const EventPayload&) {
    // LVGL v8 message box API: a NULL-terminated array of button labels,
    // one LV_EVENT_VALUE_CHANGED fired with the clicked label available via
    // lv_msgbox_get_active_btn_text() — this is NOT the v9 footer-button API.
    static const char* btns[] = {"IZBRIŠI VSE", "PREKLIČI", ""};
    lv_obj_t* mbox = lv_msgbox_create(nullptr, "TOVARNIŠKA PONASTAVITEV",
                                       "To bo izbrisalo vse uporabnike, kartice in nastavitve. Nadaljujem?",
                                       btns, false);
    lv_obj_center(mbox);
    lv_obj_add_event_cb(mbox, factoryResetMsgboxCb, LV_EVENT_VALUE_CHANGED, nullptr);
  });

  EventBus::instance().on(Topic::FactoryResetConfirmed, [](const EventPayload&) {
    EventLogger::instance().log(EventType::FACTORY_RESET, EventSource::LOCAL, "", "", "CONFIRMED");
    ConfigManager::instance().factoryReset();
    LittleFS.remove("/users.json");
    LittleFS.remove("/cards.json");
    delay(300);
    ESP.restart();
  });
}
