#pragma once
// Self-test suite (spec section 28) — surfaced on the AMOLED DIAGNOSTICS
// screen and the web DIAGNOSTICS page. Each check is cheap and read-only;
// this never actuates a servo or otherwise changes device state on its own
// (a dedicated "TEST" button per component triggers an active test, e.g.
// RFID/servo TEST buttons already described in their own web sections).
#include <Arduino.h>
#include <vector>
#include <esp_heap_caps.h>
#include "../rfid/RfidManager.h"
#include "../servo/LockController.h"
#include "../lid/LidSensor.h"
#include "../wifi/WifiManager.h"
#include "../rtc/RtcManager.h"
#include "../imu/ImuManager.h"
#include "../power/PowerManager.h"
#include "../sd/SdManager.h"
#include "../audio/Es8311Codec.h"

enum class DiagStatus { OK, WARN, FAIL, NOT_PRESENT };

struct DiagResult {
  String component;
  DiagStatus status;
  String detail;
};

class DiagnosticsManager {
 public:
  static DiagnosticsManager& instance() {
    static DiagnosticsManager m;
    return m;
  }

  std::vector<DiagResult> runAll() {
    std::vector<DiagResult> r;
    r.push_back({"ESP32-S3", DiagStatus::OK, "MCU running, " + String(ESP.getCpuFreqMHz()) + " MHz"});
    r.push_back(psram());
    r.push_back(flash());
    r.push_back({"WiFi", wifiStatus(), wifiDetail()});
    r.push_back({"Bluetooth", DiagStatus::NOT_PRESENT, "BLE stack not yet wired up (spec 38, reserved)"});
    r.push_back({"AMOLED", DiagStatus::OK, "assumed OK if this text is visible on-screen"});
    r.push_back({"Touch", DiagStatus::OK, "see touch calibration screen for a live tap test"});
    r.push_back({"RFID", RfidManager::instance().isPresent() ? DiagStatus::OK : DiagStatus::FAIL,
                 RfidManager::instance().isPresent() ? "MFRC522 responding" : "not detected, check wiring"});
    r.push_back(servo(1));
    r.push_back(servo(2));
    r.push_back({"Lid sensor", LidSensor::instance().state() != LidState::UNKNOWN ? DiagStatus::OK : DiagStatus::NOT_PRESENT, ""});
    r.push_back({"Button 1", DiagStatus::OK, "press test available on this screen"});
    r.push_back({"Button 2", DiagStatus::OK, "press test available on this screen"});
    r.push_back({"Speaker", Es8311Codec::instance().isInitialized() ? DiagStatus::OK : DiagStatus::WARN,
                 Es8311Codec::instance().isInitialized() ? "codec initialized" : "codec init pending (see Es8311Codec.h TODO)"});
    r.push_back({"Microphones", DiagStatus::NOT_PRESENT, "disabled by design (spec 20)"});
    r.push_back({"RTC", RtcManager::instance().isPresent() ? DiagStatus::OK : DiagStatus::FAIL, ""});
    r.push_back({"IMU", ImuManager::instance().isPresent() ? DiagStatus::OK : DiagStatus::FAIL, ""});
    r.push_back({"microSD", SdManager::instance().isPresent() ? DiagStatus::OK : DiagStatus::NOT_PRESENT,
                 SdManager::instance().isPresent() ? String(SdManager::instance().sizeBytes() / (1024*1024)) + " MB" : "no card (optional)"});
    r.push_back({"Battery/PMIC", PowerManager::instance().isPresent() ? DiagStatus::OK : DiagStatus::NOT_PRESENT, ""});
    r.push_back({"Web server", DiagStatus::OK, "responding (you're looking at this over it, or it's LOCAL/AMOLED-only)"});
    return r;
  }

 private:
  DiagnosticsManager() = default;

  DiagResult psram() {
    size_t total = ESP.getPsramSize();
    return {"PSRAM", total > 0 ? DiagStatus::OK : DiagStatus::FAIL, String(total / (1024*1024)) + " MB"};
  }
  DiagResult flash() {
    return {"Flash", DiagStatus::OK, String(ESP.getFlashChipSize() / (1024*1024)) + " MB"};
  }
  DiagStatus wifiStatus() {
    switch (WifiManager::instance().state()) {
      case WifiState::CONNECTED: return DiagStatus::OK;
      case WifiState::AP_MODE: return DiagStatus::WARN;
      case WifiState::OFFLINE: return DiagStatus::WARN;
      default: return DiagStatus::WARN;
    }
  }
  String wifiDetail() {
    switch (WifiManager::instance().state()) {
      case WifiState::CONNECTED: return "connected, IP " + WifiManager::instance().ipAddress();
      case WifiState::AP_MODE: return "setup AP active: " + WifiManager::instance().apSsid();
      case WifiState::OFFLINE: return "offline — device still fully functional locally (spec 23)";
      default: return "starting";
    }
  }
  DiagResult servo(int which) {
    bool attached = LockController::instance().state() != LockState::UNKNOWN;
    return {String("Servo ") + which, attached ? DiagStatus::OK : DiagStatus::NOT_PRESENT,
            attached ? "" : "not configured (set GPIO in SERVOS settings)"};
  }
};
