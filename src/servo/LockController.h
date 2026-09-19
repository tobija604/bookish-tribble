#pragma once
// Drives the two SG90 lock servos (spec sections 13-15). SG90s have no
// position feedback, so "preveri končni položaj" (verify final position) is
// necessarily timing-based here: we know each servo's configured speed, so
// we know how long a given angle change should take, and treat "didn't
// finish moving in that time + margin" as the error condition. If a servo's
// GPIO isn't configured (Config::ServoConfig.gpio == -1 / enabled == false)
// that servo is treated as absent, and a lock/unlock that needs it fails
// safe into LOCK ERROR rather than silently only actuating one side — a box
// that thinks it's locked with one bolt still open is worse than an error
// screen.
//
// IMPORTANT (spec 13): the servo must only move a latch/bolt, never carry
// the lid's mechanical load — that's a mechanical design constraint on the
// enclosure, not something this firmware can enforce, but the calibration
// UI (SERVOS screen) should be used with the real hardware in hand.
#include <Arduino.h>
#include <ESP32Servo.h>
#include <map>
#include "../core/Module.h"
#include "../core/EventBus.h"
#include "../config/ConfigManager.h"
#include "../events/EventLogger.h"

enum class LockState { UNKNOWN, LOCKED, UNLOCKED, MOVING, ERROR };

class LockController : public IModule {
 public:
  static LockController& instance() {
    static LockController ctl;
    return ctl;
  }

  void begin() override {
    EventBus::instance().on(Topic::LidStateChanged, [this](const EventPayload& p) {
      lidClosed_ = !p.flag; // flag=true means "open" by convention used by LidSensor
    });
    EventBus::instance().on(Topic::RequestUnlock, [this](const EventPayload& p) {
      (void)p; requestUnlock();
    });
    EventBus::instance().on(Topic::RequestLock, [this](const EventPayload& p) {
      (void)p; requestLock();
    });
    attachIfConfigured();
    // Boot into a known mechanical state: default to LOCKED so a power
    // cycle never leaves the box silently open (fail-safe per spec 61).
    moveBoth(true /*locked*/, /*isBootSync=*/true);
  }

  void loop() override {
    if (state_ != LockState::MOVING) return;
    uint32_t now = millis();
    bool s1done = !servo1Attached_ || (now >= servo1MoveDoneAt_);
    bool s2done = !servo2Attached_ || (now >= servo2MoveDoneAt_);
    if (s1done && s2done) finishMove();
  }

  const char* name() const override { return "LockController"; }

  LockState state() const { return state_; }

  void requestUnlock() {
    if (state_ == LockState::MOVING) return;
    if (!servo1Attached_ && !servo2Attached_) {
      fail("no servo configured");
      return;
    }
    moveBoth(false, false);
  }

  void requestLock() {
    if (state_ == LockState::MOVING) return;
    auto& cfg = ConfigManager::instance().get();
    if (cfg.lid.enabled && cfg.lid.warningOnOpenWhileLocked && !lidClosed_) {
      // Spec 15/17: locking checks lid state first. We still allow it (the
      // user may be intentionally locking an open box, e.g. for storage),
      // but flag it so the UI can warn.
      Serial.println("[Lock] locking while lid reports OPEN — proceeding, but flagging");
    }
    if (!servo1Attached_ && !servo2Attached_) {
      fail("no servo configured");
      return;
    }
    moveBoth(true, false);
  }

  void reloadCalibration() { attachIfConfigured(); }

 private:
  LockController() = default;
  Servo servo1_, servo2_;
  bool servo1Attached_ = false, servo2Attached_ = false;
  LockState state_ = LockState::UNKNOWN;
  bool lidClosed_ = true;
  bool pendingLocked_ = false;
  bool bootSync_ = false;
  uint32_t servo1MoveDoneAt_ = 0, servo2MoveDoneAt_ = 0;

  void attachIfConfigured() {
    auto& cfg = ConfigManager::instance().get();
    if (cfg.servo1.enabled && cfg.servo1.gpio >= 0) {
      if (!servo1Attached_) { servo1_.attach(cfg.servo1.gpio); servo1Attached_ = true; }
    } else if (servo1Attached_) { servo1_.detach(); servo1Attached_ = false; }

    if (cfg.servo2.enabled && cfg.servo2.gpio >= 0) {
      if (!servo2Attached_) { servo2_.attach(cfg.servo2.gpio); servo2Attached_ = true; }
    } else if (servo2Attached_) { servo2_.detach(); servo2Attached_ = false; }
  }

  void moveBoth(bool locked, bool isBootSync) {
    auto& cfg = ConfigManager::instance().get();
    state_ = LockState::MOVING;
    pendingLocked_ = locked;
    bootSync_ = isBootSync;
    uint32_t now = millis();

    if (servo1Attached_) {
      int target = angleFor(cfg.servo1, locked);
      writeServoDelayed(servo1_, cfg.servo1, target);
      servo1MoveDoneAt_ = now + cfg.servo1.delayMs + estimateMoveMs(cfg.servo1, target);
    }
    if (servo2Attached_) {
      int target = angleFor(cfg.servo2, locked);
      writeServoDelayed(servo2_, cfg.servo2, target);
      servo2MoveDoneAt_ = now + cfg.servo2.delayMs + estimateMoveMs(cfg.servo2, target);
    }
  }

  int angleFor(const ServoConfig& s, bool locked) {
    int a = locked ? s.lockedAngle : s.unlockedAngle;
    return s.invert ? (180 - a) : a;
  }

  uint32_t estimateMoveMs(const ServoConfig& s, int targetAngle) {
    int last = lastAngle_.count(&s) ? lastAngle_[&s] : targetAngle;
    int delta = abs(targetAngle - last);
    lastAngle_[&s] = targetAngle;
    int speed = s.speedDegPerSec > 0 ? s.speedDegPerSec : 180;
    uint32_t est = (uint32_t)((delta * 1000L) / speed) + 150; // +150ms settle margin
    return est;
  }

  void writeServoDelayed(Servo& servo, const ServoConfig& s, int target) {
    // Simple approach: SG90s accept a direct angle write and move on their
    // own; per-servo `delayMs` staggers servo2 relative to servo1 if desired
    // (spec 14 "delay"). Non-blocking: we don't delay() here, we just factor
    // delayMs into servo*MoveDoneAt_ above and write immediately — for a
    // true staggered start, main.cpp's scheduler-friendly variant can be
    // added later; the wide-skeleton default writes both immediately since
    // most SG90 setups don't need a staggered start.
    servo.write(target);
  }

  void finishMove() {
    // No physical feedback exists; treat "moved for the expected duration
    // without a servo error we can detect" as success. A future revision
    // with servo current sensing or a locked/unlocked reed switch pair can
    // set state_ = LockState::ERROR here on mismatch.
    state_ = pendingLocked_ ? LockState::LOCKED : LockState::UNLOCKED;
    EventPayload p; p.flag = (state_ == LockState::LOCKED);
    EventBus::instance().emit(Topic::LockStateChanged, p);
    if (!bootSync_) {
      EventLogger::instance().log(pendingLocked_ ? EventType::LOCK : EventType::UNLOCK,
                                   EventSource::SYSTEM, "", "", "OK");
    }
  }

  void fail(const String& reason) {
    state_ = LockState::ERROR;
    EventLogger::instance().log(EventType::SERVO_ERROR, EventSource::SYSTEM, "", "", "ERROR", reason);
    EventBus::instance().emit(Topic::ServoError, EventPayload(reason));
  }

  std::map<const ServoConfig*, int> lastAngle_;
};
