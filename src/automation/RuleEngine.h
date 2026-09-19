#pragma once
// IF/THEN automation rule engine (spec section 33). Rules are stored/edited
// via the web UI's AUTOMATION screen and persisted to /automation.json.
// Conditions and actions are matched against EventBus topics + simple
// state checks, so adding a new rule never requires a firmware rebuild.
//
// Supported condition kinds (extend the enum + matches() as new triggers
// are needed):
//   EVENT            -> fires when a given EventType occurs N times in a row
//                        (default N=1), e.g. "IF RFID_GRANTED"
//   TIME_OF_DAY      -> fires once per day at HH:MM, e.g. "IF TIME = 23:00"
//   REPEATED_DENIALS -> fires after N consecutive RFID_DENIED, e.g.
//                        "IF 5 RFID_DENIED"
// Supported actions: UNLOCK, LOCK, WARNING, SYNC (NTP resync), NOTIFY (logs
// a SYSTEM event only — placeholder for the future push-notification path).
#include <Arduino.h>
#include <vector>
#include <map>
#include "../core/EventBus.h"
#include "../storage/StorageManager.h"
#include "../events/Event.h"
#include "../events/EventLogger.h"

enum class RuleConditionKind { EVENT, TIME_OF_DAY, REPEATED_DENIALS };
enum class RuleActionKind { UNLOCK, LOCK, WARNING, SYNC, NOTIFY };

struct AutomationRule {
  String id;
  String label;
  bool enabled = true;
  RuleConditionKind conditionKind = RuleConditionKind::EVENT;
  EventType conditionEvent = EventType::RFID_GRANTED;
  int conditionCount = 1;         // for REPEATED_DENIALS
  uint8_t timeHour = 23, timeMinute = 0; // for TIME_OF_DAY
  RuleActionKind action = RuleActionKind::UNLOCK;
};

class RuleEngine {
 public:
  static RuleEngine& instance() {
    static RuleEngine e;
    return e;
  }

  void begin() {
    load();
    if (rules_.empty()) seedDefaults();

    EventBus::instance().on(Topic::RfidGranted, [this](const EventPayload&) { onEvent(EventType::RFID_GRANTED); });
    EventBus::instance().on(Topic::RfidDenied, [this](const EventPayload&) { onEvent(EventType::RFID_DENIED); });
    EventBus::instance().on(Topic::LidStateChanged, [this](const EventPayload& p) {
      onEvent(p.flag ? EventType::LID_OPEN : EventType::LID_CLOSED);
    });
    EventBus::instance().on(Topic::WifiConnected, [this](const EventPayload&) { onEvent(EventType::WIFI_CONNECTED); });
  }

  void loop() {
    time_t now = time(nullptr);
    struct tm t; localtime_r(&now, &t);
    if (t.tm_min == lastCheckedMinute_) return;
    lastCheckedMinute_ = t.tm_min;
    for (auto& r : rules_) {
      if (!r.enabled || r.conditionKind != RuleConditionKind::TIME_OF_DAY) continue;
      if (t.tm_hour == r.timeHour && t.tm_min == r.timeMinute) fire(r);
    }
  }

  const std::vector<AutomationRule>& all() const { return rules_; }
  void add(const AutomationRule& r) { rules_.push_back(r); save(); }
  bool remove(const String& id) {
    for (size_t i = 0; i < rules_.size(); i++)
      if (rules_[i].id == id) { rules_.erase(rules_.begin() + i); save(); return true; }
    return false;
  }
  void setEnabled(const String& id, bool en) {
    for (auto& r : rules_) if (r.id == id) { r.enabled = en; save(); return; }
  }

 private:
  RuleEngine() = default;
  std::vector<AutomationRule> rules_;
  std::map<EventType, int> denialStreaks_;
  int lastCheckedMinute_ = -1;
  static constexpr const char* kPath = "/automation.json";

  void onEvent(EventType t) {
    if (t == EventType::RFID_DENIED) denialStreaks_[t]++;
    else denialStreaks_[EventType::RFID_DENIED] = 0;

    for (auto& r : rules_) {
      if (!r.enabled) continue;
      if (r.conditionKind == RuleConditionKind::EVENT && r.conditionEvent == t) fire(r);
      if (r.conditionKind == RuleConditionKind::REPEATED_DENIALS && t == EventType::RFID_DENIED &&
          denialStreaks_[EventType::RFID_DENIED] >= r.conditionCount) {
        fire(r);
        denialStreaks_[EventType::RFID_DENIED] = 0;
      }
    }
  }

  void fire(const AutomationRule& r) {
    switch (r.action) {
      case RuleActionKind::UNLOCK: EventBus::instance().emit(Topic::RequestUnlock); break;
      case RuleActionKind::LOCK: EventBus::instance().emit(Topic::RequestLock); break;
      case RuleActionKind::WARNING:
        EventLogger::instance().log(EventType::SYSTEM_ERROR, EventSource::AUTOMATION, "", "", "WARNING", r.label);
        break;
      case RuleActionKind::SYNC: EventBus::instance().emit(Topic::WifiConnected); break; // nudges NTP resync path
      case RuleActionKind::NOTIFY:
        EventLogger::instance().log(EventType::SYSTEM_ERROR, EventSource::AUTOMATION, "", "", "NOTIFY", r.label);
        break;
    }
  }

  void seedDefaults() {
    AutomationRule r1; r1.id = "rule-1"; r1.label = "Auto-lock on lid close";
    r1.conditionKind = RuleConditionKind::EVENT; r1.conditionEvent = EventType::LID_CLOSED;
    r1.action = RuleActionKind::LOCK;
    rules_.push_back(r1);

    AutomationRule r2; r2.id = "rule-2"; r2.label = "Warn after 5 denied RFID attempts";
    r2.conditionKind = RuleConditionKind::REPEATED_DENIALS; r2.conditionCount = 5;
    r2.action = RuleActionKind::WARNING;
    rules_.push_back(r2);

    AutomationRule r3; r3.id = "rule-3"; r3.label = "Lock every day at 23:00";
    r3.conditionKind = RuleConditionKind::TIME_OF_DAY; r3.timeHour = 23; r3.timeMinute = 0;
    r3.action = RuleActionKind::LOCK;
    rules_.push_back(r3);

    save();
  }

  void load() {
    JsonDocument doc;
    rules_.clear();
    if (!StorageManager::instance().loadJson(kPath, doc) || !doc["rules"].is<JsonArray>()) return;
    for (JsonObject o : doc["rules"].as<JsonArray>()) {
      AutomationRule r;
      r.id = o["id"].as<String>(); r.label = o["label"] | "";
      r.enabled = o["enabled"] | true;
      r.conditionKind = (RuleConditionKind)(int)(o["conditionKind"] | 0);
      r.conditionEvent = (EventType)(int)(o["conditionEvent"] | 0);
      r.conditionCount = o["conditionCount"] | 1;
      r.timeHour = o["timeHour"] | 23; r.timeMinute = o["timeMinute"] | 0;
      r.action = (RuleActionKind)(int)(o["action"] | 0);
      rules_.push_back(r);
    }
  }

  void save() {
    JsonDocument doc;
    JsonArray arr = doc["rules"].to<JsonArray>();
    for (auto& r : rules_) {
      JsonObject o = arr.add<JsonObject>();
      o["id"] = r.id; o["label"] = r.label; o["enabled"] = r.enabled;
      o["conditionKind"] = (int)r.conditionKind; o["conditionEvent"] = (int)r.conditionEvent;
      o["conditionCount"] = r.conditionCount; o["timeHour"] = r.timeHour; o["timeMinute"] = r.timeMinute;
      o["action"] = (int)r.action;
    }
    StorageManager::instance().saveJson(kPath, doc);
  }
};
