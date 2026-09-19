#pragma once
// Optional onboard microSD (spec section 37): event log overflow, backups,
// custom audio, future images/diagnostics. Never a boot dependency — every
// other module already persists its own state to internal LittleFS.
#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include "../../include/pins.h"

class SdManager {
 public:
  static SdManager& instance() {
    static SdManager m;
    return m;
  }

  bool begin() {
    sdSpi_.begin(PIN_SD_SCLK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);
    present_ = SD.begin(PIN_SD_CS, sdSpi_);
    if (!present_) {
      Serial.println("[SD] no card detected — continuing without it, per spec 37 this is not a fault");
      return false;
    }
    Serial.printf("[SD] card mounted, %llu MB\n", SD.cardSize() / (1024ULL * 1024ULL));
    return true;
  }

  bool isPresent() const { return present_; }
  uint64_t sizeBytes() { return present_ ? SD.cardSize() : 0; }
  uint64_t usedBytes() { return present_ ? SD.usedBytes() : 0; }

 private:
  SdManager() = default;
  SPIClass sdSpi_{HSPI};
  bool present_ = false;
};
