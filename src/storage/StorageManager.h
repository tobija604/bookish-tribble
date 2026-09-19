#pragma once
// =============================================================================
// StorageManager — thin LittleFS wrapper used by every module that persists
// state (config, users, cards, events, backups). Centralizing this here means
// only one place needs to worry about atomic writes, so a power loss mid-save
// can't corrupt a file that other modules also depend on. microSD (sd/) is a
// separate, optional layer for logs/backups — LittleFS on internal flash is
// what everything else always has, per spec 37: "SD ne sme biti pogoj za
// osnovno delovanje".
// =============================================================================
#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

class StorageManager {
 public:
  static StorageManager& instance() {
    static StorageManager mgr;
    return mgr;
  }

  bool begin() {
    if (!LittleFS.begin(true /* formatOnFail */)) {
      Serial.println("[Storage] LittleFS mount failed even after format");
      ready_ = false;
      return false;
    }
    ready_ = true;
    return true;
  }

  bool isReady() const { return ready_; }

  // Loads a JSON document from `path` into `doc`. Returns false (and leaves
  // `doc` untouched) if the file doesn't exist or fails to parse — callers
  // must fall back to sane in-memory defaults rather than crash, since a
  // missing config file is the normal state on first boot.
  bool loadJson(const String& path, JsonDocument& doc) {
    if (!ready_ || !LittleFS.exists(path)) return false;
    File f = LittleFS.open(path, "r");
    if (!f) return false;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) {
      Serial.printf("[Storage] parse error on %s: %s\n", path.c_str(), err.c_str());
      return false;
    }
    return true;
  }

  // Atomic-ish save: write to a temp file, then rename over the target, so a
  // reset mid-write leaves either the old file or the new one, never a
  // half-written one.
  bool saveJson(const String& path, const JsonDocument& doc) {
    if (!ready_) return false;
    String tmp = path + ".tmp";
    File f = LittleFS.open(tmp, "w");
    if (!f) return false;
    size_t written = serializeJson(doc, f);
    f.close();
    if (written == 0) {
      LittleFS.remove(tmp);
      return false;
    }
    LittleFS.remove(path);
    return LittleFS.rename(tmp, path);
  }

  bool appendLine(const String& path, const String& line) {
    if (!ready_) return false;
    File f = LittleFS.open(path, "a");
    if (!f) return false;
    f.println(line);
    f.close();
    return true;
  }

  size_t usedBytes() { return LittleFS.usedBytes(); }
  size_t totalBytes() { return LittleFS.totalBytes(); }

 private:
  StorageManager() = default;
  bool ready_ = false;
};
