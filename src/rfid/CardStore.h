#pragma once
#include <Arduino.h>
#include <vector>
#include "Card.h"
#include "../storage/StorageManager.h"

class CardStore {
 public:
  static CardStore& instance() {
    static CardStore store;
    return store;
  }

  void begin() {
    JsonDocument doc;
    cards_.clear();
    if (!StorageManager::instance().loadJson(kPath, doc) || !doc["cards"].is<JsonArray>()) return;
    for (JsonObject o : doc["cards"].as<JsonArray>()) {
      RfidCard c;
      c.uid = o["uid"].as<String>();
      c.userId = o["userId"] | "";
      c.label = o["label"] | "";
      c.enabled = o["enabled"] | true;
      c.createdEpoch = o["createdEpoch"] | 0;
      c.lastUsedEpoch = o["lastUsedEpoch"] | 0;
      c.useCount = o["useCount"] | 0;
      if (o["schedule"].is<JsonObject>()) {
        JsonObject s = o["schedule"];
        c.schedule.enabled = s["enabled"] | false;
        c.schedule.allowedFromEpoch = s["allowedFromEpoch"] | 0;
        c.schedule.allowedUntilEpoch = s["allowedUntilEpoch"] | 0;
        c.schedule.weekdaysMask = s["weekdaysMask"] | 0b1111111;
        c.schedule.dailyStartMinute = s["dailyStartMinute"] | 0;
        c.schedule.dailyEndMinute = s["dailyEndMinute"] | (24 * 60);
        c.schedule.temporary = s["temporary"] | false;
      }
      cards_.push_back(c);
    }
  }

  const std::vector<RfidCard>& all() const { return cards_; }

  RfidCard* findByUid(const String& uid) {
    for (auto& c : cards_) if (c.uid.equalsIgnoreCase(uid)) return &c;
    return nullptr;
  }

  bool exists(const String& uid) { return findByUid(uid) != nullptr; }

  RfidCard& add(const String& uid, const String& userId, const String& label) {
    RfidCard c;
    c.uid = uid; c.userId = userId; c.label = label;
    c.createdEpoch = (uint32_t)time(nullptr);
    cards_.push_back(c);
    save();
    return cards_.back();
  }

  bool remove(const String& uid) {
    for (size_t i = 0; i < cards_.size(); i++) {
      if (cards_[i].uid.equalsIgnoreCase(uid)) { cards_.erase(cards_.begin() + i); save(); return true; }
    }
    return false;
  }

  void recordUse(const String& uid) {
    RfidCard* c = findByUid(uid);
    if (!c) return;
    c->lastUsedEpoch = (uint32_t)time(nullptr);
    c->useCount++;
    save();
  }

  RfidCard* mostUsed() {
    RfidCard* best = nullptr;
    for (auto& c : cards_) if (!best || c.useCount > best->useCount) best = &c;
    return best;
  }

  bool save() {
    JsonDocument doc;
    JsonArray arr = doc["cards"].to<JsonArray>();
    for (auto& c : cards_) {
      JsonObject o = arr.add<JsonObject>();
      o["uid"] = c.uid; o["userId"] = c.userId; o["label"] = c.label;
      o["enabled"] = c.enabled; o["createdEpoch"] = c.createdEpoch;
      o["lastUsedEpoch"] = c.lastUsedEpoch; o["useCount"] = c.useCount;
      JsonObject s = o["schedule"].to<JsonObject>();
      s["enabled"] = c.schedule.enabled;
      s["allowedFromEpoch"] = c.schedule.allowedFromEpoch;
      s["allowedUntilEpoch"] = c.schedule.allowedUntilEpoch;
      s["weekdaysMask"] = c.schedule.weekdaysMask;
      s["dailyStartMinute"] = c.schedule.dailyStartMinute;
      s["dailyEndMinute"] = c.schedule.dailyEndMinute;
      s["temporary"] = c.schedule.temporary;
    }
    return StorageManager::instance().saveJson(kPath, doc);
  }

 private:
  CardStore() = default;
  static constexpr const char* kPath = "/cards.json";
  std::vector<RfidCard> cards_;
};
