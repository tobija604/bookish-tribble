#pragma once
// Wraps a MFRC522 reader (spec sections 5-7). Runs as a simple state machine
// polled from loop(): IDLE -> (card presented) -> either a normal
// authorization check, or, if a web "ADD CARD" flow is pending, a UID
// capture that's handed back via a callback instead of being checked against
// the user list.
#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>
#include "../../include/pins.h"
#include "../config/Config.h"
#include "../config/HardwareConfig.h"
#include "../core/Module.h"
#include "../core/EventBus.h"
#include "../events/EventLogger.h"
#include "../users/UserManager.h"
#include "CardStore.h"

class RfidManager : public IModule {
 public:
  static RfidManager& instance() {
    static RfidManager mgr;
    return mgr;
  }

  void begin() override {
    SPI.begin(PIN_RFID_SCK, PIN_RFID_MISO, PIN_RFID_MOSI, PIN_RFID_SS);
    mfrc_ = new MFRC522(PIN_RFID_SS, PIN_RFID_RST);
    mfrc_->PCD_Init();
    byte version = mfrc_->PCD_ReadRegister(MFRC522::VersionReg);
    present_ = (version != 0x00 && version != 0xFF);
    if (!present_) {
      Serial.println("[RFID] MFRC522 not detected — check wiring (see include/pins.h notes)");
    } else {
      Serial.printf("[RFID] MFRC522 detected, version 0x%02X\n", version);
    }
  }

  void loop() override {
    if (!enabled_ || !present_) return;
    uint32_t now = millis();
    if (now - lastScanMs_ < cooldownMs_) return;

    if (!mfrc_->PICC_IsNewCardPresent() || !mfrc_->PICC_ReadCardSerial()) return;

    String uid = uidToHex(mfrc_->uid.uidByte, mfrc_->uid.size);
    lastScanMs_ = now;
    mfrc_->PICC_HaltA();

    EventBus::instance().emit(Topic::RfidCardPresented, EventPayload(uid));
    EventLogger::instance().log(EventType::RFID_SCAN, EventSource::RFID, "", uid);

    if (addCardCallback_) {
      auto cb = addCardCallback_;
      addCardCallback_ = nullptr; // one-shot
      cb(uid);
      return;
    }

    checkAccess(uid);
  }

  const char* name() const override { return "RfidManager"; }

  void setEnabled(bool en) { enabled_ = en; }
  void setCooldownMs(uint32_t ms) { cooldownMs_ = ms; }
  bool isPresent() const { return present_; }

  // Puts the reader into one-shot "capture next UID" mode for the web
  // ADD CARD flow (spec section 6). `cb` fires exactly once with the scanned
  // UID, then normal access checking resumes.
  void captureNextUid(std::function<void(const String&)> cb) { addCardCallback_ = cb; }
  bool isCapturing() const { return (bool)addCardCallback_; }

 private:
  RfidManager() = default;
  MFRC522* mfrc_ = nullptr;
  bool enabled_ = true;
  bool present_ = false;
  uint32_t lastScanMs_ = 0;
  uint32_t cooldownMs_ = 1500;
  uint16_t consecutiveDenials_ = 0;
  std::function<void(const String&)> addCardCallback_;

  String uidToHex(byte* buf, byte len) {
    String s;
    for (byte i = 0; i < len; i++) {
      if (buf[i] < 0x10) s += "0";
      s += String(buf[i], HEX);
    }
    s.toUpperCase();
    return s;
  }

  void checkAccess(const String& uid) {
    RfidCard* card = CardStore::instance().findByUid(uid);
    if (!card) {
      deny(uid, "unknown card");
      return;
    }
    if (!card->enabled) {
      deny(uid, "card disabled");
      return;
    }
    SmartBoxUser* user = UserManager::instance().findById(card->userId);
    if (!user || !user->active) {
      deny(uid, "user inactive or missing");
      return;
    }
    if (!user->hasPermission(PERM_UNLOCK)) {
      deny(uid, "user lacks unlock permission");
      return;
    }

    time_t now = time(nullptr);
    struct tm t; localtime_r(&now, &t);
    int weekdayMon0 = (t.tm_wday == 0) ? 6 : (t.tm_wday - 1);
    uint16_t minuteOfDay = t.tm_hour * 60 + t.tm_min;

    // A card-level schedule (if enabled) takes precedence over the user's
    // own schedule; otherwise the user's schedule applies (spec 34).
    if (card->schedule.enabled && !card->schedule.isWithinSchedule(now, weekdayMon0, minuteOfDay)) {
      deny(uid, "outside card schedule");
      return;
    }
    if (!card->schedule.enabled && user->schedule.enabled &&
        !user->schedule.isWithinSchedule(now, weekdayMon0, minuteOfDay)) {
      deny(uid, "outside user schedule");
      return;
    }

    grant(uid, card, user);
  }

  void grant(const String& uid, RfidCard* card, SmartBoxUser* user) {
    consecutiveDenials_ = 0;
    CardStore::instance().recordUse(uid);
    UserManager::instance().recordAccess(user->id);
    EventLogger::instance().log(EventType::RFID_GRANTED, EventSource::RFID, user->id, uid, "GRANTED");
    EventBus::instance().emit(Topic::RfidGranted, EventPayload(uid));
    EventPayload p(uid);
    p.str2 = user->id;
    EventBus::instance().emit(Topic::RequestUnlock, p);
  }

  void deny(const String& uid, const String& reason) {
    consecutiveDenials_++;
    EventLogger::instance().log(EventType::RFID_DENIED, EventSource::RFID, "", uid, "DENIED", reason);
    EventBus::instance().emit(Topic::RfidDenied, EventPayload(uid));
    // Tamper/warning threshold (spec section 54) is enforced by
    // SecurityManager, which subscribes to RfidDenied and tracks the
    // rolling failure count against SecurityConfig::maxFailedAttempts —
    // kept there rather than here so all "too many failures" logic (RFID,
    // web login, PIN) lives in one place.
  }
};
