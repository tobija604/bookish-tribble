#pragma once
#include <Arduino.h>

// Mirrors spec section 29 exactly, so nothing has to be translated between
// firmware and the web UI's EVENTS filter dropdown.
enum class EventType {
  BOOT, WIFI_CONNECTED, WIFI_DISCONNECTED, RFID_SCAN, RFID_GRANTED, RFID_DENIED,
  UNLOCK, LOCK, REMOTE_UNLOCK, LID_OPEN, LID_CLOSED, ADMIN_LOGIN, ADMIN_LOGOUT,
  WEB_LOCKED, SECURITY_CODE_CREATED, SECURITY_CODE_USED, SECURITY_CODE_EXPIRED,
  CONFIG_CHANGED, SERVO_ERROR, RFID_ERROR, FIRMWARE_UPDATE, FACTORY_RESET,
  SYSTEM_ERROR, TAMPER_DETECTED,
};

enum class EventSource { LOCAL, RFID, WEB, BUTTON, AUTOMATION, SYSTEM };

inline const char* eventTypeToStr(EventType t) {
  switch (t) {
    case EventType::BOOT: return "BOOT";
    case EventType::WIFI_CONNECTED: return "WIFI_CONNECTED";
    case EventType::WIFI_DISCONNECTED: return "WIFI_DISCONNECTED";
    case EventType::RFID_SCAN: return "RFID_SCAN";
    case EventType::RFID_GRANTED: return "RFID_GRANTED";
    case EventType::RFID_DENIED: return "RFID_DENIED";
    case EventType::UNLOCK: return "UNLOCK";
    case EventType::LOCK: return "LOCK";
    case EventType::REMOTE_UNLOCK: return "REMOTE_UNLOCK";
    case EventType::LID_OPEN: return "LID_OPEN";
    case EventType::LID_CLOSED: return "LID_CLOSED";
    case EventType::ADMIN_LOGIN: return "ADMIN_LOGIN";
    case EventType::ADMIN_LOGOUT: return "ADMIN_LOGOUT";
    case EventType::WEB_LOCKED: return "WEB_LOCKED";
    case EventType::SECURITY_CODE_CREATED: return "SECURITY_CODE_CREATED";
    case EventType::SECURITY_CODE_USED: return "SECURITY_CODE_USED";
    case EventType::SECURITY_CODE_EXPIRED: return "SECURITY_CODE_EXPIRED";
    case EventType::CONFIG_CHANGED: return "CONFIG_CHANGED";
    case EventType::SERVO_ERROR: return "SERVO_ERROR";
    case EventType::RFID_ERROR: return "RFID_ERROR";
    case EventType::FIRMWARE_UPDATE: return "FIRMWARE_UPDATE";
    case EventType::FACTORY_RESET: return "FACTORY_RESET";
    case EventType::SYSTEM_ERROR: return "SYSTEM_ERROR";
    case EventType::TAMPER_DETECTED: return "TAMPER_DETECTED";
  }
  return "UNKNOWN";
}

inline const char* eventSourceToStr(EventSource s) {
  switch (s) {
    case EventSource::LOCAL: return "LOCAL";
    case EventSource::RFID: return "RFID";
    case EventSource::WEB: return "WEB";
    case EventSource::BUTTON: return "BUTTON";
    case EventSource::AUTOMATION: return "AUTOMATION";
    case EventSource::SYSTEM: return "SYSTEM";
  }
  return "SYSTEM";
}

struct SmartBoxEvent {
  uint32_t    id = 0;
  uint32_t    timestampEpoch = 0;
  EventType   type;
  String      user;      // user id or display name, if applicable
  String      rfidUid;   // hex UID, if applicable
  String      result;    // e.g. "GRANTED", "DENIED", "OK", "ERROR"
  String      reason;    // free text, e.g. "unknown card", "schedule violation"
  EventSource source = EventSource::SYSTEM;
};
