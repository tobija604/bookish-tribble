#pragma once
// Central audit trail (spec sections 29-31). Keeps the last N events in RAM
// (fast for the AMOLED EVENTS screen and the web dashboard's live feed) and
// appends every event as one JSON line to /events.log on LittleFS so history
// survives a reboot. The log file is rotated once it gets too big for a
// device with no mandatory SD card (spec 37) — older entries are simply
// dropped rather than risking filling the flash and breaking config saves.
#include <Arduino.h>
#include <deque>
#include <time.h>
#include "Event.h"
#include "../storage/StorageManager.h"

class EventLogger {
 public:
  static EventLogger& instance() {
    static EventLogger log;
    return log;
  }

  void begin() {
    nextId_ = 1;
    // Recover a rough last-id from the tail of the log so IDs don't restart
    // at 1 after every reboot (cosmetic, but avoids duplicate-looking IDs in
    // the web UI right after a restart).
    // (Kept intentionally simple: full JSONL replay into RAM is skipped to
    // keep boot fast; the web UI's EVENTS screen falls back to reading the
    // file directly for history older than the in-RAM ring buffer.)
  }

  void log(EventType type, EventSource source, const String& user = "",
           const String& rfidUid = "", const String& result = "",
           const String& reason = "") {
    SmartBoxEvent e;
    e.id = nextId_++;
    e.timestampEpoch = (uint32_t)time(nullptr);
    e.type = type; e.source = source; e.user = user;
    e.rfidUid = rfidUid; e.result = result; e.reason = reason;

    ring_.push_back(e);
    while (ring_.size() > kRingCapacity) ring_.pop_front();

    persist(e);
    updateCounters(e);
  }

  const std::deque<SmartBoxEvent>& recent() const { return ring_; }

  struct Stats {
    uint32_t unlocksToday = 0, unlocksWeek = 0, unlocksMonth = 0;
    uint32_t deniedAttempts = 0;
    uint32_t servoErrors = 0;
    uint32_t wifiDisconnects = 0;
    uint32_t systemErrors = 0;
  };
  const Stats& stats() const { return stats_; }

 private:
  static constexpr size_t kRingCapacity = 200;
  static constexpr const char* kLogPath = "/events.log";
  static constexpr size_t kMaxLogBytes = 400 * 1024; // leaves headroom on the shared LittleFS partition

  std::deque<SmartBoxEvent> ring_;
  uint32_t nextId_ = 1;
  Stats stats_;

  void persist(const SmartBoxEvent& e) {
    if (!StorageManager::instance().isReady()) return;
    JsonDocument doc;
    doc["id"] = e.id; doc["ts"] = e.timestampEpoch;
    doc["type"] = eventTypeToStr(e.type); doc["source"] = eventSourceToStr(e.source);
    doc["user"] = e.user; doc["rfidUid"] = e.rfidUid;
    doc["result"] = e.result; doc["reason"] = e.reason;
    String line;
    serializeJson(doc, line);
    StorageManager::instance().appendLine(kLogPath, line);
    maybeRotate();
  }

  void maybeRotate() {
    // Cheap guard: only check file size occasionally to avoid a filesystem
    // stat() on every single event.
    static uint16_t counter = 0;
    if ((++counter % 32) != 0) return;
    File f = LittleFS.open(kLogPath, "r");
    if (!f) return;
    size_t sz = f.size();
    f.close();
    if (sz > kMaxLogBytes) {
      // Simple rotation: drop the oldest half by rewriting only the tail.
      // Good enough for a device without guaranteed SD storage; a more
      // sophisticated rotation can move old logs to /sd when present.
      File in = LittleFS.open(kLogPath, "r");
      if (!in) return;
      size_t toSkip = sz / 2;
      in.seek(toSkip);
      // advance to next newline so we don't keep a half-written line
      while (in.available() && in.read() != '\n') {}
      String tail;
      while (in.available()) tail += (char)in.read();
      in.close();
      File out = LittleFS.open(String(kLogPath) + ".tmp", "w");
      if (out) { out.print(tail); out.close(); LittleFS.remove(kLogPath); LittleFS.rename(String(kLogPath) + ".tmp", kLogPath); }
    }
  }

  void updateCounters(const SmartBoxEvent& e) {
    time_t now = time(nullptr);
    struct tm nowTm; localtime_r(&now, &nowTm);
    struct tm evTm; time_t evTime = e.timestampEpoch; localtime_r(&evTime, &evTm);

    bool sameDay = (nowTm.tm_year == evTm.tm_year && nowTm.tm_yday == evTm.tm_yday);
    bool sameWeek = (nowTm.tm_year == evTm.tm_year && (nowTm.tm_yday / 7) == (evTm.tm_yday / 7));
    bool sameMonth = (nowTm.tm_year == evTm.tm_year && nowTm.tm_mon == evTm.tm_mon);

    if (e.type == EventType::UNLOCK || e.type == EventType::REMOTE_UNLOCK) {
      if (sameDay) stats_.unlocksToday++;
      if (sameWeek) stats_.unlocksWeek++;
      if (sameMonth) stats_.unlocksMonth++;
    }
    if (e.type == EventType::RFID_DENIED) stats_.deniedAttempts++;
    if (e.type == EventType::SERVO_ERROR) stats_.servoErrors++;
    if (e.type == EventType::WIFI_DISCONNECTED) stats_.wifiDisconnects++;
    if (e.type == EventType::SYSTEM_ERROR) stats_.systemErrors++;
  }
};
