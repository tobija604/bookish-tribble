#pragma once
#include <Arduino.h>

enum class Role { ADMIN, MANAGER, USER, GUEST };

inline const char* roleToStr(Role r) {
  switch (r) {
    case Role::ADMIN: return "ADMIN";
    case Role::MANAGER: return "MANAGER";
    case Role::USER: return "USER";
    case Role::GUEST: return "GUEST";
  }
  return "USER";
}
inline Role roleFromStr(const String& s) {
  if (s == "ADMIN") return Role::ADMIN;
  if (s == "MANAGER") return Role::MANAGER;
  if (s == "GUEST") return Role::GUEST;
  return Role::USER;
}

// Bitmask permissions (spec section 8). Role grants a sane default set, but
// individual permissions can be overridden per user.
enum Permission : uint16_t {
  PERM_UNLOCK           = 1 << 0,
  PERM_LOCK             = 1 << 1,
  PERM_RFID_MANAGEMENT  = 1 << 2,
  PERM_USER_MANAGEMENT  = 1 << 3,
  PERM_SETTINGS         = 1 << 4,
  PERM_NETWORK          = 1 << 5,
  PERM_FIRMWARE         = 1 << 6,
  PERM_DIAGNOSTICS      = 1 << 7,
  PERM_LOGS             = 1 << 8,
  PERM_FACTORY_RESET    = 1 << 9,
};

inline uint16_t defaultPermissionsForRole(Role r) {
  switch (r) {
    case Role::ADMIN:
      return 0xFFFF;
    case Role::MANAGER:
      return PERM_UNLOCK | PERM_LOCK | PERM_RFID_MANAGEMENT | PERM_USER_MANAGEMENT |
             PERM_SETTINGS | PERM_LOGS;
    case Role::USER:
      return PERM_UNLOCK | PERM_LOCK;
    case Role::GUEST:
    default:
      return PERM_UNLOCK;
  }
}

// Per-user/card access schedule (spec section 34).
struct AccessSchedule {
  bool     enabled       = false;
  uint32_t allowedFromEpoch  = 0;  // 0 = no lower bound
  uint32_t allowedUntilEpoch = 0;  // 0 = no upper bound (permanent unless temporary)
  uint8_t  weekdaysMask   = 0b1111111; // bit0=Mon..bit6=Sun, default: every day
  uint16_t dailyStartMinute = 0;    // minutes since midnight, 0 = no restriction
  uint16_t dailyEndMinute   = 24*60;
  bool     temporary      = false;
};

struct SmartBoxUser {
  String   id;              // generated, e.g. "u-XXXXXX"
  String   firstName;
  String   lastName;
  String   username;
  String   description;
  bool     active = true;
  Role     role = Role::USER;
  uint16_t permissions = defaultPermissionsForRole(Role::USER);
  uint32_t createdEpoch = 0;
  uint32_t lastAccessEpoch = 0;
  uint32_t accessCount = 0;
  AccessSchedule schedule;
  // passwordHash is only relevant for web-login-capable roles (MANAGER/ADMIN);
  // stored as a salted hash (see security/SecurityManager::hashPassword), never plaintext.
  String   passwordHash;

  bool hasPermission(Permission p) const { return (permissions & p) != 0; }

  bool isWithinSchedule(uint32_t nowEpoch, int weekdayMonBased0, uint16_t minuteOfDay) const {
    if (!schedule.enabled) return true;
    if (schedule.allowedFromEpoch && nowEpoch < schedule.allowedFromEpoch) return false;
    if (schedule.allowedUntilEpoch && nowEpoch > schedule.allowedUntilEpoch) return false;
    if (!((schedule.weekdaysMask >> weekdayMonBased0) & 0x01)) return false;
    if (minuteOfDay < schedule.dailyStartMinute || minuteOfDay > schedule.dailyEndMinute) return false;
    return true;
  }
};
