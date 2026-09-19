#pragma once
// Maps a button's configured action name (spec sections 10-11) to real
// firmware behavior. Kept separate from ButtonManager.h so that module can
// stay a pure debouncer with no dependency on display/security/OTA code.
#include <Arduino.h>
#include "../core/EventBus.h"
#include "../security/SecurityManager.h"
#include "../wifi/WifiManager.h"
#include "../ota/OtaManager.h"
#include "../events/EventLogger.h"
#include "../config/ConfigManager.h"

class ButtonActionDispatcher {
 public:
  static ButtonActionDispatcher& instance() {
    static ButtonActionDispatcher d;
    return d;
  }

  void begin() {
    EventBus::instance().on(Topic::Button1Event, [this](const EventPayload& p) { dispatch(1, p); });
    EventBus::instance().on(Topic::Button2Event, [this](const EventPayload& p) { dispatch(2, p); });
    EventBus::instance().on(Topic::FactoryResetRequested, [this](const EventPayload&) {
      // The 10s-hold combo from ButtonManager only ARMS the reset; per spec
      // 11 it still needs AMOLED confirmation before anything destructive
      // happens. main.cpp wires this topic to a confirmation dialog on the
      // display; this handler just logs the arm event.
      EventLogger::instance().log(EventType::FACTORY_RESET, EventSource::BUTTON, "", "", "ARMED",
                                   "hold BUTTON2 10s — awaiting AMOLED confirmation");
    });
  }

  // Exposed so main.cpp's confirmation dialog can actually perform the
  // reset once the user confirms on-screen.
  void performConfirmedFactoryReset() {
    EventBus::instance().emit(Topic::FactoryResetConfirmed);
  }

 private:
  ButtonActionDispatcher() = default;

  void dispatch(int which, const EventPayload& p) {
    const String& action = p.str2;
    if (action == "SHOW_STATUS" || action == "OPEN_MENU" || action == "SHOW_DEVICE_INFO") {
      EventBus::instance().emit(which == 1 ? Topic::Button1Event : Topic::Button2Event, p);
      // main.cpp's display wiring listens for these specific action strings
      // to switch the AMOLED to the right screen; kept generic here so this
      // dispatcher doesn't need to depend on display/screens headers (that
      // would create a display<->buttons circular include).
    } else if (action == "GENERATE_SECURITY_CODE") {
      SecurityManager::instance().generateSecurityCode();
    } else if (action == "UNLOCK") {
      EventBus::instance().emit(Topic::RequestUnlock);
    } else if (action == "LOCK") {
      EventBus::instance().emit(Topic::RequestLock);
    } else if (action == "REBOOT") {
      delay(200);
      ESP.restart();
    } else if (action == "WIFI_SETUP") {
      // Re-arm AP mode on demand even if STA is currently connected —
      // useful for switching networks in the field without the web UI.
      ConfigManager::instance().get().wifi.ssid = "";
      ConfigManager::instance().save();
      ESP.restart();
    } else if (action == "DIAGNOSTICS" || action == "SERVICE_MODE" || action == "OPEN_QUICK_MENU" ||
               action == "SHOW_IP" || action == "SHOW_WIFI" || action == "CAMERA" || action == "CUSTOM_ACTION") {
      // Screen-only actions: forwarded as-is for main.cpp's display wiring
      // to interpret (CAMERA is a no-op until a camera module is present,
      // spec section 47).
    }
    (void)which; // kept in the signature for the actions above that branch on it via `p`/topic choice
  }
};
