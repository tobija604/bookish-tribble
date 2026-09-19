#pragma once
// CRUD store for SmartBoxUser, persisted to /users.json. RFID cards
// themselves live in rfid/CardStore.h and reference a SmartBoxUser::id —
// kept separate because a user can hold several cards (spec section 6: "Card
// has ... user ID") and because RFID lookups (by UID, on every scan) should
// not have to walk full user records.
#include <Arduino.h>
#include <vector>
#include "User.h"
#include "../storage/StorageManager.h"

class UserManager {
 public:
  static UserManager& instance() {
    static UserManager mgr;
    return mgr;
  }

  void begin() {
    JsonDocument doc;
    users_.clear();
    if (StorageManager::instance().loadJson(kPath, doc) && doc["users"].is<JsonArray>()) {
      for (JsonObject o : doc["users"].as<JsonArray>()) {
        SmartBoxUser u;
        u.id = o["id"].as<String>();
        u.firstName = o["firstName"] | "";
        u.lastName = o["lastName"] | "";
        u.username = o["username"] | "";
        u.description = o["description"] | "";
        u.active = o["active"] | true;
        u.role = roleFromStr(o["role"] | "USER");
        u.permissions = o["permissions"] | defaultPermissionsForRole(u.role);
        u.createdEpoch = o["createdEpoch"] | 0;
        u.lastAccessEpoch = o["lastAccessEpoch"] | 0;
        u.accessCount = o["accessCount"] | 0;
        u.passwordHash = o["passwordHash"] | "";
        if (o["schedule"].is<JsonObject>()) {
          JsonObject s = o["schedule"];
          u.schedule.enabled = s["enabled"] | false;
          u.schedule.allowedFromEpoch = s["allowedFromEpoch"] | 0;
          u.schedule.allowedUntilEpoch = s["allowedUntilEpoch"] | 0;
          u.schedule.weekdaysMask = s["weekdaysMask"] | 0b1111111;
          u.schedule.dailyStartMinute = s["dailyStartMinute"] | 0;
          u.schedule.dailyEndMinute = s["dailyEndMinute"] | (24 * 60);
          u.schedule.temporary = s["temporary"] | false;
        }
        users_.push_back(u);
      }
    }
    if (users_.empty()) {
      // First boot: seed a single local ADMIN account so the web UI is never
      // unreachable. Username/password should be changed immediately —
      // SETTINGS > USERS surfaces a persistent warning until it is.
      SmartBoxUser admin;
      admin.id = "u-000001";
      admin.username = "admin";
      admin.firstName = "Admin";
      admin.role = Role::ADMIN;
      admin.permissions = defaultPermissionsForRole(Role::ADMIN);
      admin.active = true;
      admin.createdEpoch = 0;
      users_.push_back(admin);
      save();
      Serial.println("[Users] seeded default admin account (username: admin) — change its password");
    }
  }

  const std::vector<SmartBoxUser>& all() const { return users_; }

  SmartBoxUser* findById(const String& id) {
    for (auto& u : users_) if (u.id == id) return &u;
    return nullptr;
  }
  SmartBoxUser* findByUsername(const String& username) {
    for (auto& u : users_) if (u.username == username) return &u;
    return nullptr;
  }

  SmartBoxUser& create(const String& firstName, const String& lastName,
                        const String& username, Role role) {
    SmartBoxUser u;
    u.id = generateId();
    u.firstName = firstName; u.lastName = lastName; u.username = username;
    u.role = role; u.permissions = defaultPermissionsForRole(role);
    u.createdEpoch = (uint32_t)time(nullptr);
    users_.push_back(u);
    save();
    return users_.back();
  }

  bool remove(const String& id) {
    for (size_t i = 0; i < users_.size(); i++) {
      if (users_[i].id == id) { users_.erase(users_.begin() + i); save(); return true; }
    }
    return false;
  }

  void recordAccess(const String& id) {
    SmartBoxUser* u = findById(id);
    if (!u) return;
    u->lastAccessEpoch = (uint32_t)time(nullptr);
    u->accessCount++;
    save();
  }

  bool save() {
    JsonDocument doc;
    JsonArray arr = doc["users"].to<JsonArray>();
    for (auto& u : users_) {
      JsonObject o = arr.add<JsonObject>();
      o["id"] = u.id; o["firstName"] = u.firstName; o["lastName"] = u.lastName;
      o["username"] = u.username; o["description"] = u.description;
      o["active"] = u.active; o["role"] = roleToStr(u.role); o["permissions"] = u.permissions;
      o["createdEpoch"] = u.createdEpoch; o["lastAccessEpoch"] = u.lastAccessEpoch;
      o["accessCount"] = u.accessCount; o["passwordHash"] = u.passwordHash;
      JsonObject s = o["schedule"].to<JsonObject>();
      s["enabled"] = u.schedule.enabled;
      s["allowedFromEpoch"] = u.schedule.allowedFromEpoch;
      s["allowedUntilEpoch"] = u.schedule.allowedUntilEpoch;
      s["weekdaysMask"] = u.schedule.weekdaysMask;
      s["dailyStartMinute"] = u.schedule.dailyStartMinute;
      s["dailyEndMinute"] = u.schedule.dailyEndMinute;
      s["temporary"] = u.schedule.temporary;
    }
    return StorageManager::instance().saveJson(kPath, doc);
  }

 private:
  UserManager() = default;
  static constexpr const char* kPath = "/users.json";
  std::vector<SmartBoxUser> users_;

  String generateId() {
    uint32_t n = (uint32_t)esp_random();
    char buf[16];
    snprintf(buf, sizeof(buf), "u-%06lx", (unsigned long)(n & 0xFFFFFF));
    return String(buf);
  }
};
