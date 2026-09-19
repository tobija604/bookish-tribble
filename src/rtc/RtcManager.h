#pragma once
// Onboard PCF85063 RTC (spec section 21) via lewisxhe/SensorLib. Time keeps
// working with zero network connectivity as long as the RTC is powered;
// WifiManager's NTP sync (see wifi/WifiManager.h) periodically corrects
// drift once online, and calls RtcManager::setFromSystemTime() so the RTC
// itself stays accurate across the next offline stretch.
//
// NOTE: verify SensorPCF85063's exact API against the installed SensorLib
// version — constructor/init signatures have changed between releases.
#include <Arduino.h>
#include <Wire.h>
#include <time.h>
#include <SensorPCF85063.hpp>
#include "../../include/pins.h"

class RtcManager {
 public:
  static RtcManager& instance() {
    static RtcManager m;
    return m;
  }

  bool begin() {
    present_ = rtc_.begin(Wire, PCF85063_SLAVE_ADDRESS, PIN_I2C_SDA, PIN_I2C_SCL);
    if (!present_) {
      Serial.println("[RTC] PCF85063 not detected on shared I2C bus — falling back to system clock only "
                      "(time will reset on every reboot until WiFi/NTP or the RTC wiring is fixed)");
      return false;
    }
    syncSystemTimeFromRtc();
    return true;
  }

  bool isPresent() const { return present_; }

  void syncSystemTimeFromRtc() {
    if (!present_) return;
    RTC_DateTime dt = rtc_.getDateTime();
    struct tm t{};
    t.tm_year = dt.year - 1900; t.tm_mon = dt.month - 1; t.tm_mday = dt.day;
    t.tm_hour = dt.hour; t.tm_min = dt.minute; t.tm_sec = dt.second;
    time_t epoch = mktime(&t);
    struct timeval tv{ .tv_sec = epoch, .tv_usec = 0 };
    settimeofday(&tv, nullptr);
  }

  void setFromSystemTime() {
    if (!present_) return;
    time_t now = time(nullptr);
    struct tm t; localtime_r(&now, &t);
    rtc_.setDateTime(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
  }

 private:
  RtcManager() = default;
  SensorPCF85063 rtc_;
  bool present_ = false;
};
