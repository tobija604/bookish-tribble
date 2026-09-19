#pragma once
// First-boot AP provisioning + normal STA connection + NTP + graceful
// offline operation (spec sections 22-23). WiFi is explicitly optional:
// every other module is written to keep working with WifiManager sitting in
// Offline/ApMode indefinitely.
#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include "../core/Module.h"
#include "../core/EventBus.h"
#include "../config/ConfigManager.h"
#include "../events/EventLogger.h"

enum class WifiState { BOOT, AP_MODE, CONNECTING, CONNECTED, OFFLINE };

class WifiManager : public IModule {
 public:
  static WifiManager& instance() {
    static WifiManager m;
    return m;
  }

  void begin() override {
    auto& wcfg = ConfigManager::instance().get().wifi;
    WiFi.setHostname(wcfg.hostname.c_str());

    if (wcfg.ssid.isEmpty()) {
      startApMode();
      return;
    }
    startStaConnect();
  }

  void loop() override {
    switch (state_) {
      case WifiState::CONNECTING: {
        if (WiFi.status() == WL_CONNECTED) {
          onConnected();
        } else if (millis() - connectStartMs_ > 20000UL) {
          Serial.println("[WiFi] STA connect timed out, staying offline (RFID/servo/touch/buttons keep working)");
          state_ = WifiState::OFFLINE;
        }
        break;
      }
      case WifiState::CONNECTED: {
        if (WiFi.status() != WL_CONNECTED) {
          state_ = WifiState::OFFLINE;
          EventLogger::instance().log(EventType::WIFI_DISCONNECTED, EventSource::SYSTEM);
          EventBus::instance().emit(Topic::WifiDisconnected);
        } else if (!ntpSynced_ && millis() - connectedAtMs_ > 2000UL) {
          syncNtp();
        }
        break;
      }
      case WifiState::OFFLINE: {
        // Periodic silent reconnect attempts, never blocking.
        if (millis() - lastReconnectAttemptMs_ > 30000UL && !ConfigManager::instance().get().wifi.ssid.isEmpty()) {
          lastReconnectAttemptMs_ = millis();
          startStaConnect();
        }
        break;
      }
      default: break;
    }
  }

  const char* name() const override { return "WifiManager"; }

  WifiState state() const { return state_; }
  String apSsid() const { return apSsid_; }
  String ipAddress() const {
    return (state_ == WifiState::CONNECTED) ? WiFi.localIP().toString()
         : (state_ == WifiState::AP_MODE)   ? WiFi.softAPIP().toString()
                                             : String("0.0.0.0");
  }
  String macAddress() const { return WiFi.macAddress(); }

  // Called by the web SETUP flow once the user submits SSID/password.
  bool provision(const String& ssid, const String& password, const String& hostname,
                  const String& deviceName, const String& timezone) {
    auto& cfg = ConfigManager::instance().get();
    cfg.wifi.ssid = ssid; cfg.wifi.password = password;
    if (hostname.length()) cfg.wifi.hostname = hostname;
    if (deviceName.length()) cfg.wifi.deviceName = deviceName;
    if (timezone.length()) cfg.wifi.timezone = timezone;
    cfg.provisioned = true;
    ConfigManager::instance().save();
    startStaConnect();
    return true;
  }

 private:
  WifiManager() = default;
  WifiState state_ = WifiState::BOOT;
  String apSsid_;
  uint32_t connectStartMs_ = 0, connectedAtMs_ = 0, lastReconnectAttemptMs_ = 0;
  bool ntpSynced_ = false;

  void startApMode() {
    uint32_t suffix = (uint32_t)(ESP.getEfuseMac() & 0xFFFF);
    char buf[24]; snprintf(buf, sizeof(buf), "SMARTBOX-%04X", (unsigned)suffix);
    apSsid_ = buf;
    WiFi.mode(WIFI_AP);
    WiFi.softAP(apSsid_.c_str()); // open network for first-boot setup, per spec 22
    state_ = WifiState::AP_MODE;
    Serial.printf("[WiFi] AP mode: SSID=%s IP=%s\n", apSsid_.c_str(), WiFi.softAPIP().toString().c_str());
    EventBus::instance().emit(Topic::WifiApStarted, EventPayload(apSsid_));
  }

  void startStaConnect() {
    auto& wcfg = ConfigManager::instance().get().wifi;
    if (wcfg.ssid.isEmpty()) { startApMode(); return; }
    WiFi.mode(WIFI_STA);
    WiFi.begin(wcfg.ssid.c_str(), wcfg.password.c_str());
    state_ = WifiState::CONNECTING;
    connectStartMs_ = millis();
  }

  void onConnected() {
    state_ = WifiState::CONNECTED;
    connectedAtMs_ = millis();
    ntpSynced_ = false;
    // AP is only for first-boot setup; once STA connects, switch off the AP
    // entirely (spec 22: "AP se izklopi").
    if (WiFi.getMode() != WIFI_STA) WiFi.mode(WIFI_STA);
    EventLogger::instance().log(EventType::WIFI_CONNECTED, EventSource::SYSTEM);
    EventBus::instance().emit(Topic::WifiConnected, EventPayload(WiFi.localIP().toString()));
    Serial.printf("[WiFi] connected, IP=%s\n", WiFi.localIP().toString().c_str());
  }

  void syncNtp() {
    auto& wcfg = ConfigManager::instance().get().wifi;
    // RTC (rtc/RtcManager) is the primary time source and works with no
    // network at all; NTP here just periodically corrects RTC drift once
    // online, per spec 21.
    configTzTime(posixTzFromIana(wcfg.timezone).c_str(), wcfg.ntpServer.c_str());
    ntpSynced_ = true;
  }

  // Minimal IANA->POSIX TZ mapping for the handful of zones this project is
  // likely to run in; extend as needed. Falls back to fixed CET/CEST for
  // anything unrecognized, which matches Europe/Ljubljana anyway.
  String posixTzFromIana(const String& iana) {
    if (iana == "Europe/Ljubljana" || iana == "Europe/Vienna" || iana == "Europe/Berlin" ||
        iana == "Europe/Zagreb" || iana == "Europe/Rome") {
      return "CET-1CEST,M3.5.0,M10.5.0/3";
    }
    if (iana == "UTC") return "UTC0";
    return "CET-1CEST,M3.5.0,M10.5.0/3";
  }
};
