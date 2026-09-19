#pragma once
// Onboard QMI8658 accelerometer+gyro (spec section 39) via SensorLib, used
// for anti-tamper detection: if the box is LOCKED and the IMU reports
// motion/shock past a threshold, emit TamperDetected (already wired to
// sound + event log via SecurityManager/AudioManager subscribers).
//
// NOTE: verify SensorQMI8658's exact API against the installed SensorLib
// version; accel scale/units and register names vary by release.
#include <Arduino.h>
#include <Wire.h>
#include <SensorQMI8658.hpp>
#include "../../include/pins.h"
#include "../core/EventBus.h"
#include "../servo/LockController.h"

class ImuManager {
 public:
  static ImuManager& instance() {
    static ImuManager m;
    return m;
  }

  bool begin() {
    present_ = imu_.begin(Wire, QMI8658_L_SLAVE_ADDRESS, PIN_I2C_SDA, PIN_I2C_SCL);
    if (!present_) {
      Serial.println("[IMU] QMI8658 not detected — tamper detection disabled");
      return false;
    }
    imu_.configAccelerometer(SensorQMI8658::ACC_RANGE_4G, SensorQMI8658::ACC_ODR_250Hz);
    imu_.enableAccelerometer();
    baselineMag_ = readAccelMagnitude();
    return true;
  }

  void loop() {
    if (!present_) return;
    uint32_t now = millis();
    if (now - lastSampleMs_ < 100) return; // 10Hz is plenty for shock detection
    lastSampleMs_ = now;

    float mag = readAccelMagnitude();
    float delta = fabsf(mag - baselineMag_);
    baselineMag_ = baselineMag_ * 0.9f + mag * 0.1f; // slow-moving baseline, rejects gravity-orientation drift

    if (LockController::instance().state() == LockState::LOCKED && delta > kShockThresholdG) {
      uint32_t nowMs = millis();
      if (nowMs - lastTamperEventMs_ > 5000UL) { // don't spam repeated events for one shake
        lastTamperEventMs_ = nowMs;
        EventBus::instance().emit(Topic::TamperDetected, EventPayload("imu_motion"));
      }
    }
  }

  bool isPresent() const { return present_; }

 private:
  ImuManager() = default;
  SensorQMI8658 imu_;
  bool present_ = false;
  float baselineMag_ = 1.0f;
  uint32_t lastSampleMs_ = 0, lastTamperEventMs_ = 0;
  static constexpr float kShockThresholdG = 0.6f; // tune once real hardware is in hand

  float readAccelMagnitude() {
    float ax = 0, ay = 0, az = 0;
    if (!imu_.getAccelerometer(ax, ay, az)) return baselineMag_;
    return sqrtf(ax * ax + ay * ay + az * az);
  }
};
