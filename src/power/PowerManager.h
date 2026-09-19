#pragma once
// Onboard AXP2101 PMIC (spec section 40) via lewisxhe/XPowersLib. Reports
// battery voltage/percentage/charging state to the web ABOUT/DASHBOARD
// screens; if a reading looks unreliable (no battery fitted, e.g. USB-only
// deployments), isBatteryPresent() lets the UI hide the battery widget
// instead of showing a misleading "0%".
//
// NOTE: verify XPowersAXP2101's exact API against the installed
// XPowersLib version.
#include <Arduino.h>
#include <Wire.h>
#include <XPowersLib.h>
#include "../../include/pins.h"
#include "../core/EventBus.h"

class PowerManager {
 public:
  static PowerManager& instance() {
    static PowerManager m;
    return m;
  }

  bool begin() {
    present_ = pmu_.begin(Wire, AXP2101_SLAVE_ADDRESS, PIN_I2C_SDA, PIN_I2C_SCL);
    if (!present_) {
      Serial.println("[Power] AXP2101 not detected");
      return false;
    }
    return true;
  }

  void loop() {
    if (!present_) return;
    uint32_t now = millis();
    if (now - lastCheckMs_ < 5000) return;
    lastCheckMs_ = now;

    bool wasLow = lowBattery_;
    int pct = batteryPercent();
    lowBattery_ = (isBatteryPresent() && pct >= 0 && pct <= kLowBatteryPercent);
    if (lowBattery_ && !wasLow) {
      EventBus::instance().emit(Topic::SystemError, EventPayload("low_battery"));
    }
  }

  bool isPresent() const { return present_; }
  bool isBatteryPresent() { return present_ && pmu_.isBatteryConnect(); }
  bool isCharging() { return present_ && pmu_.isCharging(); }
  float batteryVoltage() { return present_ ? pmu_.getBattVoltage() / 1000.0f : 0.0f; }
  int batteryPercent() { return present_ ? pmu_.getBatteryPercent() : -1; }
  bool isLowBattery() const { return lowBattery_; }

 private:
  PowerManager() = default;
  XPowersAXP2101 pmu_;
  bool present_ = false;
  bool lowBattery_ = false;
  uint32_t lastCheckMs_ = 0;
  static constexpr int kLowBatteryPercent = 15;
};
