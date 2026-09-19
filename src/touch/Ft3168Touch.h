#pragma once
// FT3168 capacitive touch (spec section 2), I2C, FocalTech register layout
// (shared with the common FT5x06/FT6336 family: 0x02 = touch count,
// 0x03/0x04 = X high/low, 0x05/0x06 = Y high/low for point 1). Confirm the
// exact register map against the FT3168 datasheet / Waveshare demo before
// relying on multi-touch or gesture registers — only single-point tap
// coordinates are read here, which is all LVGL's default indev needs.
#include <Arduino.h>
#include <Wire.h>
#include "../../include/pins.h"

class Ft3168Touch {
 public:
  static Ft3168Touch& instance() {
    static Ft3168Touch t;
    return t;
  }

  bool begin() {
    pinMode(PIN_TOUCH_RESET, OUTPUT);
    digitalWrite(PIN_TOUCH_RESET, LOW); delay(10);
    digitalWrite(PIN_TOUCH_RESET, HIGH); delay(50);
    pinMode(PIN_TOUCH_INT, INPUT);

    Wire.beginTransmission(kI2cAddr);
    present_ = (Wire.endTransmission() == 0);
    if (!present_) Serial.println("[Touch] FT3168 not detected on shared I2C bus");
    return present_;
  }

  // Returns true if a finger is down, filling x/y with panel coordinates
  // (0..465 on this 466x466 display).
  bool read(int16_t& x, int16_t& y) {
    if (!present_) return false;
    uint8_t reg[7];
    if (!readRegs(0x00, reg, 7)) return false;
    uint8_t touchCount = reg[2] & 0x0F;
    if (touchCount == 0) return false;
    x = ((reg[3] & 0x0F) << 8) | reg[4];
    y = ((reg[5] & 0x0F) << 8) | reg[6];
    return true;
  }

  bool isPresent() const { return present_; }

 private:
  Ft3168Touch() = default;
  static constexpr uint8_t kI2cAddr = 0x38; // common FocalTech default; confirm against board schematic
  bool present_ = false;

  bool readRegs(uint8_t startReg, uint8_t* out, size_t n) {
    Wire.beginTransmission(kI2cAddr);
    Wire.write(startReg);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom((int)kI2cAddr, (int)n) != (int)n) return false;
    for (size_t i = 0; i < n; i++) out[i] = Wire.read();
    return true;
  }
};
