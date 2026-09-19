#pragma once
// ES8311 codec I2C control-register init — TODO: port from the official
// Waveshare demo (github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.75),
// which already contains a working init sequence for this exact board. The
// ES8311 needs its I2C control path (address 0x18, shared bus — see
// include/pins.h) configured for: clock source, PLL for the sample rate
// AudioManager uses, DAC (speaker) path enabled, ADC (mic) path left
// disabled per spec section 20, and output volume/mute control instead of
// (or in addition to) AudioManager's software-level amplitude scaling.
//
// Until this is ported, AudioManager's I2S bus still comes up and drives
// PIN_AUDIO_PA_EN, but the codec chip itself is left in its power-on-reset
// state — most ES8311 boards do NOT pass audio through un-initialized, so
// expect silence (not distortion) until this class is filled in.
#include <Arduino.h>
#include <Wire.h>
#include "../../include/pins.h"

class Es8311Codec {
 public:
  static Es8311Codec& instance() {
    static Es8311Codec c;
    return c;
  }

  static constexpr uint8_t kI2cAddr = 0x18;

  bool begin() {
    Wire.beginTransmission(kI2cAddr);
    bool present = (Wire.endTransmission() == 0);
    if (!present) {
      Serial.println("[ES8311] codec not answering on shared I2C bus");
      return false;
    }
    // TODO: real register init sequence goes here (reset, clock manager,
    // ADC/DAC power, format, volume). See class-level comment.
    initialized_ = false;
    return initialized_;
  }

  bool isInitialized() const { return initialized_; }

 private:
  Es8311Codec() = default;
  bool initialized_ = false;
};
