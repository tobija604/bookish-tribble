#pragma once
// =============================================================================
// EventBus — lightweight in-memory pub/sub used to decouple modules so that
// (per spec section 59) no component blocks another: RfidManager fires
// "rfid.granted", LockController subscribes and unlocks; it never calls
// LockController directly. This is orthogonal to EventLogger (events/), which
// is the persisted audit trail — EventBus is transient, in-RAM, same-loop
// notification plumbing; most EventBus events are also forwarded into
// EventLogger by main.cpp's wiring so they show up in EVENTS/STATISTICS too.
// =============================================================================
#include <Arduino.h>
#include <functional>
#include <vector>
#include <map>

enum class Topic {
  BootComplete,
  WifiConnected,
  WifiDisconnected,
  WifiApStarted,
  RfidCardPresented,   // raw UID seen, before authorization check
  RfidGranted,
  RfidDenied,
  RequestUnlock,       // any source (touch/RFID/web/button/automation) asking to unlock
  RequestLock,
  LockStateChanged,    // payload: locked/unlocked/error
  LidStateChanged,     // payload: open/closed
  ServoError,
  Button1Event,        // payload encodes short/long/double
  Button2Event,
  SecurityCodeCreated,
  SecurityCodeUsed,
  SecurityCodeExpired,
  AdminLogin,
  AdminLogout,
  WebSessionLocked,
  ConfigChanged,
  TamperDetected,
  FactoryResetRequested,
  FactoryResetConfirmed,
  OtaStarted,
  OtaFinished,
  SystemError,
};

struct EventPayload {
  String str;      // free-form string payload (uid, reason, user id, ...)
  String str2;
  int    value = 0;
  bool   flag  = false;
  EventPayload() = default;
  EventPayload(const String& s) : str(s) {}
};

class EventBus {
 public:
  using Handler = std::function<void(const EventPayload&)>;

  static EventBus& instance() {
    static EventBus bus;
    return bus;
  }

  void on(Topic topic, Handler handler) {
    handlers_[topic].push_back(std::move(handler));
  }

  void emit(Topic topic, const EventPayload& payload = EventPayload()) {
    auto it = handlers_.find(topic);
    if (it == handlers_.end()) return;
    // Copy the handler list pointer-stability isn't guaranteed if a handler
    // subscribes during dispatch; subscriptions only happen at begin() time
    // in this codebase, so a plain iteration is safe and avoids extra heap
    // churn on every emit (this runs in the hot loop for touch/RFID/etc).
    for (auto& h : it->second) h(payload);
  }

 private:
  EventBus() = default;
  std::map<Topic, std::vector<Handler>> handlers_;
};
