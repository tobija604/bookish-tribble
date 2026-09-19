#pragma once
// Micro switch / reed / Hall sensor on the lid (spec section 16-17).
// Debounced digital input; on CLOSED, optionally triggers auto-lock after a
// configurable delay.
#include <Arduino.h>
#include "../core/Module.h"
#include "../core/EventBus.h"
#include "../config/ConfigManager.h"
#include "../config/HardwareConfig.h"
#include "../events/EventLogger.h"

enum class LidState { UNKNOWN, OPEN, CLOSED };

class LidSensor : public IModule {
 public:
  static LidSensor& instance() {
    static LidSensor s;
    return s;
  }

  void begin() override {
    auto& cfg = ConfigManager::instance().get().lid;
    if (cfg.enabled && cfg.gpio >= 0) {
      HardwareConfig::instance().pinMode(cfg.gpio, INPUT_PULLUP);
      state_ = readRaw() ? LidState::CLOSED : LidState::OPEN;
      lastRaw_ = readRaw();
    }
  }

  void loop() override {
    auto& cfg = ConfigManager::instance().get().lid;
    if (!cfg.enabled || cfg.gpio < 0) return;

    bool raw = readRaw();
    uint32_t now = millis();
    if (raw != lastRaw_) { lastChangeMs_ = now; lastRaw_ = raw; }

    if (now - lastChangeMs_ >= (uint32_t)cfg.debounceMs) {
      LidState newState = raw ? LidState::CLOSED : LidState::OPEN;
      if (newState != state_) {
        state_ = newState;
        onStateChanged(cfg);
      }
    }

    // Auto-lock timer, running independently of the debounce above.
    if (cfg.autoLock && state_ == LidState::CLOSED && autoLockPending_) {
      if (now >= autoLockAtMs_) {
        autoLockPending_ = false;
        EventBus::instance().emit(Topic::RequestLock);
      }
    }
  }

  const char* name() const override { return "LidSensor"; }
  LidState state() const { return state_; }

 private:
  LidSensor() = default;
  LidState state_ = LidState::UNKNOWN;
  bool lastRaw_ = true;
  uint32_t lastChangeMs_ = 0;
  bool autoLockPending_ = false;
  uint32_t autoLockAtMs_ = 0;

  bool readRaw() {
    auto& cfg = ConfigManager::instance().get().lid;
    int v = HardwareConfig::instance().digitalRead(cfg.gpio);
    bool closed = cfg.inverted ? (v == LOW) : (v == HIGH);
    return closed;
  }

  void onStateChanged(const LidConfig& cfg) {
    EventPayload p; p.flag = (state_ == LidState::OPEN); // flag = "is open"
    EventBus::instance().emit(Topic::LidStateChanged, p);

    if (cfg.logging) {
      EventLogger::instance().log(state_ == LidState::OPEN ? EventType::LID_OPEN : EventType::LID_CLOSED,
                                   EventSource::LOCAL);
    }

    if (state_ == LidState::CLOSED && cfg.autoLock) {
      autoLockPending_ = true;
      autoLockAtMs_ = millis() + (uint32_t)cfg.autoLockDelaySec * 1000UL;
    } else {
      autoLockPending_ = false;
    }
  }
};
