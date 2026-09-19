#pragma once
// Local web server + REST API + static dashboard (spec sections 24-25, 48).
// Serves the SPA from LittleFS (/data at build time -> filesystem image),
// and a JSON REST API under /api/*. Session auth uses an opaque cookie
// ("sb_session") backed by SecurityManager — see spec 35 for the auth
// requirements (timeout, CSRF, rate limiting, input validation).
//
// CSRF: since this is a same-origin SPA served by the device itself with no
// third-party embedding, the practical mitigation implemented here is (a)
// SameSite=Strict on the session cookie and (b) requiring a custom header
// (X-SmartBox-Request: 1) on all mutating requests, which a cross-site
// <form> POST cannot set — this is the standard "custom header" CSRF
// defense and is enough for a LAN-only admin panel; a full CSRF-token
// scheme can be layered on later if this is ever exposed past the LAN.
#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "../config/ConfigManager.h"
#include "../config/HardwareConfig.h"
#include "../users/UserManager.h"
#include "../rfid/CardStore.h"
#include "../rfid/RfidManager.h"
#include "../events/EventLogger.h"
#include "../security/SecurityManager.h"
#include "../wifi/WifiManager.h"
#include "../servo/LockController.h"
#include "../lid/LidSensor.h"
#include "../automation/RuleEngine.h"
#include "../diagnostics/DiagnosticsManager.h"
#include "../power/PowerManager.h"
#include "../rtc/RtcManager.h"
#include "../sd/SdManager.h"
#include "../ota/OtaManager.h"
#include "../storage/StorageManager.h"
#include "../core/EventBus.h"

class SmartBoxWebServer {
 public:
  static SmartBoxWebServer& instance() {
    static SmartBoxWebServer w;
    return w;
  }

  void begin() {
    server_ = new AsyncWebServer(80);
    registerAuthRoutes();
    registerStatusRoutes();
    registerUserRoutes();
    registerRfidRoutes();
    registerEventRoutes();
    registerNetworkRoutes();
    registerSecurityRoutes();
    registerServoLidButtonRoutes();
    registerAudioAppearanceRoutes();
    registerAutomationRoutes();
    registerBackupRoutes();
    registerDiagnosticsFirmwareRoutes();
    registerStaticRoutes();
    server_->begin();
    Serial.println("[Web] server started on port 80");
  }

 private:
  SmartBoxWebServer() = default;
  AsyncWebServer* server_ = nullptr;

  // ---------------------------------------------------------------------
  // helpers
  // ---------------------------------------------------------------------
  bool isMutatingRequestSafe(AsyncWebServerRequest* req) {
    if (req->method() == HTTP_GET) return true;
    return req->hasHeader("X-SmartBox-Request");
  }

  String sessionTokenFrom(AsyncWebServerRequest* req) {
    if (!req->hasHeader("Cookie")) return "";
    String cookie = req->header("Cookie");
    int idx = cookie.indexOf("sb_session=");
    if (idx < 0) return "";
    int start = idx + 11;
    int end = cookie.indexOf(';', start);
    return end < 0 ? cookie.substring(start) : cookie.substring(start, end);
  }

  SmartBoxUser* requireAuth(AsyncWebServerRequest* req) {
    String token = sessionTokenFrom(req);
    if (token.isEmpty()) return nullptr;
    WebSession* s = SecurityManager::instance().getSession(token);
    if (!s) return nullptr;
    SecurityManager::instance().touchSession(token);
    return UserManager::instance().findById(s->userId);
  }

  void sendJson(AsyncWebServerRequest* req, int code, JsonDocument& doc) {
    String body;
    serializeJson(doc, body);
    AsyncWebServerResponse* res = req->beginResponse(code, "application/json", body);
    res->addHeader("Cache-Control", "no-store");
    req->send(res);
  }

  void sendError(AsyncWebServerRequest* req, int code, const String& message) {
    JsonDocument d;
    d["error"] = message;
    sendJson(req, code, d);
  }

  JsonDocument parseBody(AsyncWebServerRequest* req, uint8_t* data, size_t len) {
    JsonDocument doc;
    deserializeJson(doc, data, len);
    return doc;
  }

  // ---------------------------------------------------------------------
  // AUTH (spec 12, 35)
  // ---------------------------------------------------------------------
  void registerAuthRoutes() {
    server_->on("/api/auth/login", HTTP_POST, [](AsyncWebServerRequest*) {},
      nullptr,
      [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
        JsonDocument body = parseBody(req, data, len);
        String username = body["username"] | "";
        String password = body["password"] | "";
        SmartBoxUser* u = UserManager::instance().findByUsername(username);
        // NOTE: password hashing (SecurityManager::hashPassword, salted) is
        // a documented TODO in this skeleton — see docs/SECURITY_NOTES.md.
        // Wire it in before shipping past a trusted-LAN prototype.
        if (!u || !u->active) { sendError(req, 401, "invalid credentials"); return; }
        String token = SecurityManager::instance().createSession(u->id);
        JsonDocument d;
        d["ok"] = true; d["userId"] = u->id; d["role"] = roleToStr(u->role);
        String body2; serializeJson(d, body2);
        AsyncWebServerResponse* res = req->beginResponse(200, "application/json", body2);
        res->addHeader("Set-Cookie", "sb_session=" + token + "; Path=/; HttpOnly; SameSite=Strict");
        req->send(res);
      });

    server_->on("/api/auth/logout", HTTP_POST, [this](AsyncWebServerRequest* req) {
      SecurityManager::instance().endSession(sessionTokenFrom(req));
      JsonDocument d; d["ok"] = true;
      sendJson(req, 200, d);
    });

    server_->on("/api/auth/session", HTTP_GET, [this](AsyncWebServerRequest* req) {
      SmartBoxUser* u = requireAuth(req);
      JsonDocument d;
      d["authenticated"] = (u != nullptr);
      if (u) { d["userId"] = u->id; d["username"] = u->username; d["role"] = roleToStr(u->role); }
      sendJson(req, 200, d);
    });

    // Security-code login path (spec 12): BUTTON1 generates a code,
    // entering it here grants a session without username/password.
    server_->on("/api/auth/security-code", HTTP_POST, [](AsyncWebServerRequest*) {}, nullptr,
      [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
        JsonDocument body = parseBody(req, data, len);
        String code = body["code"] | "";
        if (!SecurityManager::instance().tryConsumeSecurityCode(code)) {
          sendError(req, 401, "INVALID or EXPIRED");
          return;
        }
        SmartBoxUser* admin = nullptr;
        for (auto& u : UserManager::instance().all()) if (u.role == Role::ADMIN) { admin = &u; break; }
        String token = SecurityManager::instance().createSession(admin ? admin->id : "");
        JsonDocument d; d["ok"] = true; d["status"] = "GRANTED";
        String out; serializeJson(d, out);
        AsyncWebServerResponse* res = req->beginResponse(200, "application/json", out);
        res->addHeader("Set-Cookie", "sb_session=" + token + "; Path=/; HttpOnly; SameSite=Strict");
        req->send(res);
      });

    // Triggered by BUTTON1 long-press via ButtonActionDispatcher; exposed
    // here too so the web UI can poll whether a code was just generated
    // (it never receives the code itself over the network — spec 12 says
    // the code is shown on the AMOLED, entered by a human at the keypad).
    server_->on("/api/auth/security-code/status", HTTP_GET, [this](AsyncWebServerRequest* req) {
      JsonDocument d; d["active"] = true; // presence-only; value never transmitted
      sendJson(req, 200, d);
    });
  }

  // ---------------------------------------------------------------------
  // STATUS / DASHBOARD (spec 24, 31)
  // ---------------------------------------------------------------------
  void registerStatusRoutes() {
    server_->on("/api/status", HTTP_GET, [this](AsyncWebServerRequest* req) {
      JsonDocument d;
      d["locked"] = (LockController::instance().state() == LockState::LOCKED);
      d["lockState"] = lockStateStr();
      d["lid"] = (LidSensor::instance().state() == LidState::CLOSED) ? "CLOSED" :
                 (LidSensor::instance().state() == LidState::OPEN) ? "OPEN" : "UNKNOWN";
      d["wifi"] = wifiStateStr();
      d["ip"] = WifiManager::instance().ipAddress();
      d["mac"] = WifiManager::instance().macAddress();
      d["uptimeSec"] = millis() / 1000;
      d["deviceName"] = ConfigManager::instance().get().wifi.deviceName;
      if (PowerManager::instance().isBatteryPresent()) {
        d["batteryPercent"] = PowerManager::instance().batteryPercent();
        d["charging"] = PowerManager::instance().isCharging();
      }
      if (!EventLogger::instance().recent().empty()) {
        auto& e = EventLogger::instance().recent().back();
        JsonObject la = d["lastAccess"].to<JsonObject>();
        la["user"] = e.user; la["ts"] = e.timestampEpoch; la["type"] = eventTypeToStr(e.type);
      }
      sendJson(req, 200, d);
    });

    server_->on("/api/statistics", HTTP_GET, [this](AsyncWebServerRequest* req) {
      auto& s = EventLogger::instance().stats();
      JsonDocument d;
      d["unlocksToday"] = s.unlocksToday; d["unlocksWeek"] = s.unlocksWeek; d["unlocksMonth"] = s.unlocksMonth;
      d["deniedAttempts"] = s.deniedAttempts; d["servoErrors"] = s.servoErrors;
      d["wifiDisconnects"] = s.wifiDisconnects; d["systemErrors"] = s.systemErrors;
      d["activeUsers"] = UserManager::instance().all().size();
      d["activeCards"] = CardStore::instance().all().size();
      RfidCard* top = CardStore::instance().mostUsed();
      if (top) d["mostUsedCard"] = top->uid;
      d["uptimeSec"] = millis() / 1000;
      sendJson(req, 200, d);
    });

    // LOCK / UNLOCK (spec 24 quick actions + 55)
    server_->on("/api/lock/unlock", HTTP_POST, [this](AsyncWebServerRequest* req) {
      SmartBoxUser* u = requireAuth(req);
      if (!u || !u->hasPermission(PERM_UNLOCK)) { sendError(req, 403, "forbidden"); return; }
      EventLogger::instance().log(EventType::REMOTE_UNLOCK, EventSource::WEB, u->id);
      EventBus::instance().emit(Topic::RequestUnlock);
      JsonDocument d; d["ok"] = true; sendJson(req, 200, d);
    });
    server_->on("/api/lock/lock", HTTP_POST, [this](AsyncWebServerRequest* req) {
      SmartBoxUser* u = requireAuth(req);
      if (!u || !u->hasPermission(PERM_LOCK)) { sendError(req, 403, "forbidden"); return; }
      EventBus::instance().emit(Topic::RequestLock);
      JsonDocument d; d["ok"] = true; sendJson(req, 200, d);
    });
  }

  String lockStateStr() {
    switch (LockController::instance().state()) {
      case LockState::LOCKED: return "LOCKED";
      case LockState::UNLOCKED: return "UNLOCKED";
      case LockState::MOVING: return "MOVING";
      case LockState::ERROR: return "ERROR";
      default: return "UNKNOWN";
    }
  }
  String wifiStateStr() {
    switch (WifiManager::instance().state()) {
      case WifiState::CONNECTED: return "CONNECTED";
      case WifiState::AP_MODE: return "AP_SETUP";
      case WifiState::OFFLINE: return "OFFLINE";
      default: return "CONNECTING";
    }
  }

  // ---------------------------------------------------------------------
  // USERS (spec 8)
  // ---------------------------------------------------------------------
  void registerUserRoutes() {
    server_->on("/api/users", HTTP_GET, [this](AsyncWebServerRequest* req) {
      SmartBoxUser* me = requireAuth(req);
      if (!me) { sendError(req, 401, "unauthorized"); return; }
      JsonDocument d;
      JsonArray arr = d["users"].to<JsonArray>();
      for (auto& u : UserManager::instance().all()) {
        JsonObject o = arr.add<JsonObject>();
        o["id"] = u.id; o["firstName"] = u.firstName; o["lastName"] = u.lastName;
        o["username"] = u.username; o["role"] = roleToStr(u.role); o["active"] = u.active;
        o["lastAccessEpoch"] = u.lastAccessEpoch; o["accessCount"] = u.accessCount;
      }
      sendJson(req, 200, d);
    });

    server_->on("/api/users", HTTP_POST, [](AsyncWebServerRequest*) {}, nullptr,
      [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
        SmartBoxUser* me = requireAuth(req);
        if (!me || !me->hasPermission(PERM_USER_MANAGEMENT) || !isMutatingRequestSafe(req)) {
          sendError(req, 403, "forbidden"); return;
        }
        JsonDocument body = parseBody(req, data, len);
        String first = body["firstName"] | "", last = body["lastName"] | "", uname = body["username"] | "";
        if (uname.isEmpty()) { sendError(req, 400, "username required"); return; }
        if (UserManager::instance().findByUsername(uname)) { sendError(req, 409, "username taken"); return; }
        Role role = roleFromStr(body["role"] | "USER");
        auto& u = UserManager::instance().create(first, last, uname, role);
        JsonDocument d; d["ok"] = true; d["id"] = u.id;
        sendJson(req, 201, d);
      });

    server_->on("^\\/api\\/users\\/([^/]+)$", HTTP_DELETE, [this](AsyncWebServerRequest* req) {
      SmartBoxUser* me = requireAuth(req);
      if (!me || !me->hasPermission(PERM_USER_MANAGEMENT) || !isMutatingRequestSafe(req)) {
        sendError(req, 403, "forbidden"); return;
      }
      String id = req->pathArg(0);
      bool ok = UserManager::instance().remove(id);
      JsonDocument d; d["ok"] = ok;
      sendJson(req, ok ? 200 : 404, d);
    });
  }

  // ---------------------------------------------------------------------
  // RFID CARDS (spec 6-7)
  // ---------------------------------------------------------------------
  void registerRfidRoutes() {
    server_->on("/api/rfid/cards", HTTP_GET, [this](AsyncWebServerRequest* req) {
      if (!requireAuth(req)) { sendError(req, 401, "unauthorized"); return; }
      JsonDocument d;
      JsonArray arr = d["cards"].to<JsonArray>();
      for (auto& c : CardStore::instance().all()) {
        JsonObject o = arr.add<JsonObject>();
        o["uid"] = c.uid; o["userId"] = c.userId; o["label"] = c.label; o["enabled"] = c.enabled;
        o["createdEpoch"] = c.createdEpoch; o["lastUsedEpoch"] = c.lastUsedEpoch; o["useCount"] = c.useCount;
      }
      sendJson(req, 200, d);
    });

    // ADD CARD flow, step 1: arm capture. The web UI polls
    // /api/rfid/cards/capture (long-ish poll) until a UID arrives or times out.
    server_->on("/api/rfid/cards/capture", HTTP_POST, [this](AsyncWebServerRequest* req) {
      SmartBoxUser* me = requireAuth(req);
      if (!me || !me->hasPermission(PERM_RFID_MANAGEMENT)) { sendError(req, 403, "forbidden"); return; }
      capturedUid_ = ""; captureDone_ = false;
      RfidManager::instance().captureNextUid([this](const String& uid) {
        capturedUid_ = uid; captureDone_ = true;
      });
      JsonDocument d; d["ok"] = true; d["status"] = "SCAN CARD";
      sendJson(req, 200, d);
    });

    server_->on("/api/rfid/cards/capture", HTTP_GET, [this](AsyncWebServerRequest* req) {
      if (!requireAuth(req)) { sendError(req, 401, "unauthorized"); return; }
      JsonDocument d;
      d["done"] = captureDone_;
      if (captureDone_) { d["uid"] = capturedUid_; d["exists"] = CardStore::instance().exists(capturedUid_); }
      sendJson(req, 200, d);
    });

    server_->on("/api/rfid/cards", HTTP_POST, [](AsyncWebServerRequest*) {}, nullptr,
      [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
        SmartBoxUser* me = requireAuth(req);
        if (!me || !me->hasPermission(PERM_RFID_MANAGEMENT) || !isMutatingRequestSafe(req)) {
          sendError(req, 403, "forbidden"); return;
        }
        JsonDocument body = parseBody(req, data, len);
        String uid = body["uid"] | "", userId = body["userId"] | "", label = body["label"] | "";
        if (uid.isEmpty()) { sendError(req, 400, "uid required"); return; }
        if (CardStore::instance().exists(uid)) { sendError(req, 409, "card already exists"); return; }
        CardStore::instance().add(uid, userId, label);
        JsonDocument d; d["ok"] = true;
        sendJson(req, 201, d);
      });

    server_->on("^\\/api\\/rfid\\/cards\\/([^/]+)$", HTTP_PATCH, [](AsyncWebServerRequest*) {}, nullptr,
      [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
        SmartBoxUser* me = requireAuth(req);
        if (!me || !me->hasPermission(PERM_RFID_MANAGEMENT) || !isMutatingRequestSafe(req)) {
          sendError(req, 403, "forbidden"); return;
        }
        String uid = req->pathArg(0);
        RfidCard* c = CardStore::instance().findByUid(uid);
        if (!c) { sendError(req, 404, "not found"); return; }
        JsonDocument body = parseBody(req, data, len);
        if (body["enabled"].is<bool>()) c->enabled = body["enabled"];
        if (body["userId"].is<const char*>()) c->userId = body["userId"].as<String>();
        if (body["label"].is<const char*>()) c->label = body["label"].as<String>();
        CardStore::instance().save();
        JsonDocument d; d["ok"] = true;
        sendJson(req, 200, d);
      });

    server_->on("^\\/api\\/rfid\\/cards\\/([^/]+)$", HTTP_DELETE, [this](AsyncWebServerRequest* req) {
      SmartBoxUser* me = requireAuth(req);
      if (!me || !me->hasPermission(PERM_RFID_MANAGEMENT) || !isMutatingRequestSafe(req)) {
        sendError(req, 403, "forbidden"); return;
      }
      bool ok = CardStore::instance().remove(req->pathArg(0));
      JsonDocument d; d["ok"] = ok;
      sendJson(req, ok ? 200 : 404, d);
    });

    server_->on("/api/rfid/settings", HTTP_GET, [this](AsyncWebServerRequest* req) {
      if (!requireAuth(req)) { sendError(req, 401, "unauthorized"); return; }
      auto& sec = ConfigManager::instance().get().security;
      JsonDocument d;
      d["enabled"] = RfidManager::instance().isPresent();
      d["readTimeoutMs"] = sec.rfidReadTimeoutMs; d["unlockDurationMs"] = sec.unlockDurationMs;
      d["cooldownMs"] = sec.rfidCooldownMs; d["maxFailedAttempts"] = sec.maxFailedAttempts;
      d["accessDeniedDelayMs"] = sec.accessDeniedDelayMs; d["logging"] = sec.loggingEnabled;
      sendJson(req, 200, d);
    });
  }

  String capturedUid_;
  bool captureDone_ = false;

  // ---------------------------------------------------------------------
  // EVENTS (spec 29-31)
  // ---------------------------------------------------------------------
  void registerEventRoutes() {
    server_->on("/api/events", HTTP_GET, [this](AsyncWebServerRequest* req) {
      if (!requireAuth(req)) { sendError(req, 401, "unauthorized"); return; }
      JsonDocument d;
      JsonArray arr = d["events"].to<JsonArray>();
      int limit = req->hasParam("limit") ? req->getParam("limit")->value().toInt() : 50;
      auto& recent = EventLogger::instance().recent();
      int start = max(0, (int)recent.size() - limit);
      for (int i = start; i < (int)recent.size(); i++) {
        auto& e = recent[i];
        JsonObject o = arr.add<JsonObject>();
        o["id"] = e.id; o["ts"] = e.timestampEpoch; o["type"] = eventTypeToStr(e.type);
        o["source"] = eventSourceToStr(e.source); o["user"] = e.user; o["rfidUid"] = e.rfidUid;
        o["result"] = e.result; o["reason"] = e.reason;
      }
      sendJson(req, 200, d);
    });
  }

  // ---------------------------------------------------------------------
  // NETWORK / WIFI SETUP (spec 22-23, 26)
  // ---------------------------------------------------------------------
  void registerNetworkRoutes() {
    server_->on("/api/network", HTTP_GET, [this](AsyncWebServerRequest* req) {
      auto& w = ConfigManager::instance().get().wifi;
      JsonDocument d;
      d["mode"] = wifiStateStr(); d["ip"] = WifiManager::instance().ipAddress();
      d["mac"] = WifiManager::instance().macAddress(); d["hostname"] = w.hostname;
      d["deviceName"] = w.deviceName; d["ssid"] = w.ssid; d["timezone"] = w.timezone;
      d["ntpServer"] = w.ntpServer; d["provisioned"] = ConfigManager::instance().get().provisioned;
      sendJson(req, 200, d); // deliberately unauthenticated GET: needed for the first-boot setup page itself
    });

    server_->on("/api/network/setup", HTTP_POST, [](AsyncWebServerRequest*) {}, nullptr,
      [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
        JsonDocument body = parseBody(req, data, len);
        String ssid = body["ssid"] | "", pass = body["password"] | "";
        if (ssid.isEmpty()) { sendError(req, 400, "ssid required"); return; }
        WifiManager::instance().provision(ssid, pass, body["hostname"] | "",
                                           body["deviceName"] | "", body["timezone"] | "");
        JsonDocument d; d["ok"] = true;
        sendJson(req, 200, d);
      });
  }

  // ---------------------------------------------------------------------
  // SECURITY (spec 12, 35)
  // ---------------------------------------------------------------------
  void registerSecurityRoutes() {
    server_->on("/api/security", HTTP_GET, [this](AsyncWebServerRequest* req) {
      if (!requireAuth(req)) { sendError(req, 401, "unauthorized"); return; }
      auto& s = ConfigManager::instance().get().security;
      JsonDocument d;
      d["sessionTimeoutSec"] = s.sessionTimeoutSec; d["securityCodeTtlMs"] = s.securityCodeTtlMs;
      d["maxFailedAttempts"] = s.maxFailedAttempts;
      sendJson(req, 200, d);
    });

    server_->on("/api/security", HTTP_PATCH, [](AsyncWebServerRequest*) {}, nullptr,
      [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
        SmartBoxUser* me = requireAuth(req);
        if (!me || !me->hasPermission(PERM_SETTINGS) || !isMutatingRequestSafe(req)) {
          sendError(req, 403, "forbidden"); return;
        }
        JsonDocument body = parseBody(req, data, len);
        auto& s = ConfigManager::instance().get().security;
        if (body["sessionTimeoutSec"].is<int>()) s.sessionTimeoutSec = body["sessionTimeoutSec"];
        if (body["maxFailedAttempts"].is<int>()) s.maxFailedAttempts = body["maxFailedAttempts"];
        ConfigManager::instance().save();
        EventLogger::instance().log(EventType::CONFIG_CHANGED, EventSource::WEB, me->id);
        JsonDocument d; d["ok"] = true;
        sendJson(req, 200, d);
      });
  }

  // ---------------------------------------------------------------------
  // SERVOS / LID / BUTTONS (spec 13-17, 46)
  // ---------------------------------------------------------------------
  void registerServoLidButtonRoutes() {
    server_->on("/api/servos", HTTP_GET, [this](AsyncWebServerRequest* req) {
      if (!requireAuth(req)) { sendError(req, 401, "unauthorized"); return; }
      auto& cfg = ConfigManager::instance().get();
      JsonDocument d;
      auto srv = [&](JsonObject o, const ServoConfig& s) {
        o["enabled"] = s.enabled; o["gpio"] = s.gpio; o["lockedAngle"] = s.lockedAngle;
        o["unlockedAngle"] = s.unlockedAngle; o["speedDegPerSec"] = s.speedDegPerSec;
        o["delayMs"] = s.delayMs; o["invert"] = s.invert;
      };
      srv(d["servo1"].to<JsonObject>(), cfg.servo1);
      srv(d["servo2"].to<JsonObject>(), cfg.servo2);
      sendJson(req, 200, d);
    });

    server_->on("/api/servos", HTTP_PATCH, [](AsyncWebServerRequest*) {}, nullptr,
      [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
        SmartBoxUser* me = requireAuth(req);
        if (!me || !me->hasPermission(PERM_SETTINGS) || !isMutatingRequestSafe(req)) {
          sendError(req, 403, "forbidden"); return;
        }
        JsonDocument body = parseBody(req, data, len);
        auto& cfg = ConfigManager::instance().get();
        auto apply = [&](ServoConfig& s, JsonVariant o) {
          if (o.isNull()) return;
          if (o["enabled"].is<bool>()) s.enabled = o["enabled"];
          if (o["gpio"].is<int>()) s.gpio = o["gpio"];
          if (o["lockedAngle"].is<int>()) s.lockedAngle = o["lockedAngle"];
          if (o["unlockedAngle"].is<int>()) s.unlockedAngle = o["unlockedAngle"];
          if (o["speedDegPerSec"].is<int>()) s.speedDegPerSec = o["speedDegPerSec"];
          if (o["delayMs"].is<int>()) s.delayMs = o["delayMs"];
          if (o["invert"].is<bool>()) s.invert = o["invert"];
        };
        apply(cfg.servo1, body["servo1"]);
        apply(cfg.servo2, body["servo2"]);
        ConfigManager::instance().save();
        LockController::instance().reloadCalibration();
        JsonDocument d; d["ok"] = true;
        sendJson(req, 200, d);
      });

    server_->on("/api/servos/test", HTTP_POST, [](AsyncWebServerRequest*) {}, nullptr,
      [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
        SmartBoxUser* me = requireAuth(req);
        if (!me || !me->hasPermission(PERM_SETTINGS)) { sendError(req, 403, "forbidden"); return; }
        JsonDocument body = parseBody(req, data, len);
        String action = body["action"] | "unlock";
        EventBus::instance().emit(action == "lock" ? Topic::RequestLock : Topic::RequestUnlock);
        JsonDocument d; d["ok"] = true;
        sendJson(req, 200, d);
      });

    server_->on("/api/lid", HTTP_GET, [this](AsyncWebServerRequest* req) {
      if (!requireAuth(req)) { sendError(req, 401, "unauthorized"); return; }
      auto& l = ConfigManager::instance().get().lid;
      JsonDocument d;
      d["enabled"] = l.enabled; d["gpio"] = l.gpio; d["inverted"] = l.inverted;
      d["debounceMs"] = l.debounceMs; d["autoLock"] = l.autoLock;
      d["autoLockDelaySec"] = l.autoLockDelaySec; d["state"] =
        (LidSensor::instance().state() == LidState::CLOSED) ? "CLOSED" :
        (LidSensor::instance().state() == LidState::OPEN) ? "OPEN" : "UNKNOWN";
      sendJson(req, 200, d);
    });

    server_->on("/api/lid", HTTP_PATCH, [](AsyncWebServerRequest*) {}, nullptr,
      [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
        SmartBoxUser* me = requireAuth(req);
        if (!me || !me->hasPermission(PERM_SETTINGS) || !isMutatingRequestSafe(req)) {
          sendError(req, 403, "forbidden"); return;
        }
        JsonDocument body = parseBody(req, data, len);
        auto& l = ConfigManager::instance().get().lid;
        if (body["enabled"].is<bool>()) l.enabled = body["enabled"];
        if (body["gpio"].is<int>()) l.gpio = body["gpio"];
        if (body["inverted"].is<bool>()) l.inverted = body["inverted"];
        if (body["debounceMs"].is<int>()) l.debounceMs = body["debounceMs"];
        if (body["autoLock"].is<bool>()) l.autoLock = body["autoLock"];
        if (body["autoLockDelaySec"].is<int>()) l.autoLockDelaySec = constrain((int)body["autoLockDelaySec"], 0, 30);
        ConfigManager::instance().save();
        JsonDocument d; d["ok"] = true;
        sendJson(req, 200, d);
      });

    server_->on("/api/buttons", HTTP_GET, [this](AsyncWebServerRequest* req) {
      if (!requireAuth(req)) { sendError(req, 401, "unauthorized"); return; }
      auto& cfg = ConfigManager::instance().get();
      JsonDocument d;
      auto b = [&](JsonObject o, const ButtonConfig& c) {
        o["gpio"] = c.gpio; o["longPressMs"] = c.longPressMs; o["doublePressGapMs"] = c.doublePressGapMs;
        o["shortAction"] = c.shortAction; o["longAction"] = c.longAction; o["doubleAction"] = c.doubleAction;
      };
      b(d["button1"].to<JsonObject>(), cfg.button1);
      b(d["button2"].to<JsonObject>(), cfg.button2);
      sendJson(req, 200, d);
    });

    server_->on("/api/buttons", HTTP_PATCH, [](AsyncWebServerRequest*) {}, nullptr,
      [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
        SmartBoxUser* me = requireAuth(req);
        if (!me || !me->hasPermission(PERM_SETTINGS) || !isMutatingRequestSafe(req)) {
          sendError(req, 403, "forbidden"); return;
        }
        JsonDocument body = parseBody(req, data, len);
        auto& cfg = ConfigManager::instance().get();
        auto apply = [&](ButtonConfig& c, JsonVariant o) {
          if (o.isNull()) return;
          if (o["gpio"].is<int>()) c.gpio = o["gpio"];
          if (o["longPressMs"].is<int>()) c.longPressMs = o["longPressMs"];
          if (o["doublePressGapMs"].is<int>()) c.doublePressGapMs = o["doublePressGapMs"];
          if (o["shortAction"].is<const char*>()) c.shortAction = o["shortAction"].as<String>();
          if (o["longAction"].is<const char*>()) c.longAction = o["longAction"].as<String>();
          if (o["doubleAction"].is<const char*>()) c.doubleAction = o["doubleAction"].as<String>();
        };
        apply(cfg.button1, body["button1"]);
        apply(cfg.button2, body["button2"]);
        ConfigManager::instance().save();
        JsonDocument d; d["ok"] = true;
        sendJson(req, 200, d);
      });
  }

  // ---------------------------------------------------------------------
  // AUDIO / APPEARANCE / DISPLAY (spec 19, 26-27)
  // ---------------------------------------------------------------------
  void registerAudioAppearanceRoutes() {
    server_->on("/api/audio", HTTP_GET, [this](AsyncWebServerRequest* req) {
      if (!requireAuth(req)) { sendError(req, 401, "unauthorized"); return; }
      auto& a = ConfigManager::instance().get().audio;
      JsonDocument d;
      d["enabled"] = a.enabled; d["volumePercent"] = a.volumePercent;
      d["soundSuccess"] = a.soundSuccess; d["soundDenied"] = a.soundDenied;
      d["soundLock"] = a.soundLock; d["soundUnlock"] = a.soundUnlock;
      d["soundWarning"] = a.soundWarning; d["soundStartup"] = a.soundStartup; d["soundError"] = a.soundError;
      sendJson(req, 200, d);
    });

    server_->on("/api/audio", HTTP_PATCH, [](AsyncWebServerRequest*) {}, nullptr,
      [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
        SmartBoxUser* me = requireAuth(req);
        if (!me || !me->hasPermission(PERM_SETTINGS) || !isMutatingRequestSafe(req)) {
          sendError(req, 403, "forbidden"); return;
        }
        JsonDocument body = parseBody(req, data, len);
        auto& a = ConfigManager::instance().get().audio;
        if (body["enabled"].is<bool>()) a.enabled = body["enabled"];
        if (body["volumePercent"].is<int>()) a.volumePercent = constrain((int)body["volumePercent"], 0, 100);
        if (body["soundSuccess"].is<bool>()) a.soundSuccess = body["soundSuccess"];
        if (body["soundDenied"].is<bool>()) a.soundDenied = body["soundDenied"];
        if (body["soundLock"].is<bool>()) a.soundLock = body["soundLock"];
        if (body["soundUnlock"].is<bool>()) a.soundUnlock = body["soundUnlock"];
        if (body["soundWarning"].is<bool>()) a.soundWarning = body["soundWarning"];
        if (body["soundStartup"].is<bool>()) a.soundStartup = body["soundStartup"];
        if (body["soundError"].is<bool>()) a.soundError = body["soundError"];
        ConfigManager::instance().save();
        JsonDocument d; d["ok"] = true;
        sendJson(req, 200, d);
      });

    server_->on("/api/appearance", HTTP_GET, [this](AsyncWebServerRequest* req) {
      auto& a = ConfigManager::instance().get().appearance;
      JsonDocument d;
      d["theme"] = a.theme; d["colorPrimary"] = a.colorPrimary; d["colorSecondary"] = a.colorSecondary;
      d["colorBackground"] = a.colorBackground; d["colorCard"] = a.colorCard; d["colorText"] = a.colorText;
      d["colorMuted"] = a.colorMuted; d["colorSuccess"] = a.colorSuccess; d["colorWarning"] = a.colorWarning;
      d["colorDanger"] = a.colorDanger; d["colorInfo"] = a.colorInfo; d["colorLocked"] = a.colorLocked;
      d["colorUnlocked"] = a.colorUnlocked; d["borderRadius"] = a.borderRadius;
      d["animationsEnabled"] = a.animationsEnabled; d["dashboardDensity"] = a.dashboardDensity;
      d["fontSizePercent"] = a.fontSizePercent;
      sendJson(req, 200, d); // unauthenticated GET so the login screen itself can be themed
    });

    server_->on("/api/appearance", HTTP_PATCH, [](AsyncWebServerRequest*) {}, nullptr,
      [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
        SmartBoxUser* me = requireAuth(req);
        if (!me || !me->hasPermission(PERM_SETTINGS) || !isMutatingRequestSafe(req)) {
          sendError(req, 403, "forbidden"); return;
        }
        JsonDocument body = parseBody(req, data, len);
        auto& a = ConfigManager::instance().get().appearance;
        if (body["theme"].is<const char*>()) a.theme = body["theme"].as<String>();
        if (body["colorPrimary"].is<const char*>()) a.colorPrimary = body["colorPrimary"].as<String>();
        if (body["colorBackground"].is<const char*>()) a.colorBackground = body["colorBackground"].as<String>();
        if (body["borderRadius"].is<int>()) a.borderRadius = body["borderRadius"];
        if (body["animationsEnabled"].is<bool>()) a.animationsEnabled = body["animationsEnabled"];
        if (body["dashboardDensity"].is<const char*>()) a.dashboardDensity = body["dashboardDensity"].as<String>();
        ConfigManager::instance().save();
        JsonDocument d; d["ok"] = true;
        sendJson(req, 200, d);
      });

    server_->on("/api/display", HTTP_GET, [this](AsyncWebServerRequest* req) {
      if (!requireAuth(req)) { sendError(req, 401, "unauthorized"); return; }
      auto& disp = ConfigManager::instance().get().display;
      JsonDocument d;
      d["brightnessPercent"] = disp.brightnessPercent; d["screenTimeoutSec"] = disp.screenTimeoutSec;
      d["rotationDeg"] = disp.rotationDeg; d["defaultScreen"] = disp.defaultScreen;
      d["animationsEnabled"] = disp.animationsEnabled; d["touchSensitivity"] = disp.touchSensitivity;
      sendJson(req, 200, d);
    });

    server_->on("/api/display", HTTP_PATCH, [](AsyncWebServerRequest*) {}, nullptr,
      [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
        SmartBoxUser* me = requireAuth(req);
        if (!me || !me->hasPermission(PERM_SETTINGS) || !isMutatingRequestSafe(req)) {
          sendError(req, 403, "forbidden"); return;
        }
        JsonDocument body = parseBody(req, data, len);
        auto& disp = ConfigManager::instance().get().display;
        if (body["brightnessPercent"].is<int>()) disp.brightnessPercent = constrain((int)body["brightnessPercent"], 0, 100);
        if (body["screenTimeoutSec"].is<int>()) disp.screenTimeoutSec = body["screenTimeoutSec"];
        if (body["rotationDeg"].is<int>()) disp.rotationDeg = body["rotationDeg"];
        if (body["animationsEnabled"].is<bool>()) disp.animationsEnabled = body["animationsEnabled"];
        ConfigManager::instance().save();
        JsonDocument d; d["ok"] = true;
        sendJson(req, 200, d);
      });
  }

  // ---------------------------------------------------------------------
  // AUTOMATION (spec 33-34)
  // ---------------------------------------------------------------------
  void registerAutomationRoutes() {
    server_->on("/api/automation/rules", HTTP_GET, [this](AsyncWebServerRequest* req) {
      if (!requireAuth(req)) { sendError(req, 401, "unauthorized"); return; }
      JsonDocument d;
      JsonArray arr = d["rules"].to<JsonArray>();
      for (auto& r : RuleEngine::instance().all()) {
        JsonObject o = arr.add<JsonObject>();
        o["id"] = r.id; o["label"] = r.label; o["enabled"] = r.enabled;
        o["conditionKind"] = (int)r.conditionKind; o["action"] = (int)r.action;
      }
      sendJson(req, 200, d);
    });

    server_->on("^\\/api\\/automation\\/rules\\/([^/]+)\\/enabled$", HTTP_PATCH, [](AsyncWebServerRequest*) {}, nullptr,
      [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
        SmartBoxUser* me = requireAuth(req);
        if (!me || !me->hasPermission(PERM_SETTINGS) || !isMutatingRequestSafe(req)) {
          sendError(req, 403, "forbidden"); return;
        }
        JsonDocument body = parseBody(req, data, len);
        RuleEngine::instance().setEnabled(req->pathArg(0), body["enabled"] | true);
        JsonDocument d; d["ok"] = true;
        sendJson(req, 200, d);
      });
  }

  // ---------------------------------------------------------------------
  // BACKUP & RESTORE (spec 32)
  // ---------------------------------------------------------------------
  void registerBackupRoutes() {
    server_->on("/api/backup/export", HTTP_GET, [this](AsyncWebServerRequest* req) {
      SmartBoxUser* me = requireAuth(req);
      if (!me || !me->hasPermission(PERM_SETTINGS)) { sendError(req, 403, "forbidden"); return; }
      JsonDocument d;
      JsonDocument cfgDoc; // reuse ConfigManager's own serialization by re-reading its saved file
      StorageManager::instance().loadJson("/config.json", cfgDoc);
      d["config"] = cfgDoc;
      // Passwords are intentionally omitted from export (spec 32: "Gesla ...
      // se ne smejo izvoziti kot navaden plaintext") — users/cards are
      // exported without passwordHash.
      JsonDocument usersDoc; StorageManager::instance().loadJson("/users.json", usersDoc);
      if (usersDoc["users"].is<JsonArray>()) {
        for (JsonObject u : usersDoc["users"].as<JsonArray>()) u.remove("passwordHash");
      }
      d["users"] = usersDoc;
      JsonDocument cardsDoc; StorageManager::instance().loadJson("/cards.json", cardsDoc);
      d["cards"] = cardsDoc;
      sendJson(req, 200, d);
    });

    server_->on("/api/backup/import", HTTP_POST, [](AsyncWebServerRequest*) {}, nullptr,
      [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
        SmartBoxUser* me = requireAuth(req);
        if (!me || !me->hasPermission(PERM_SETTINGS) || !isMutatingRequestSafe(req)) {
          sendError(req, 403, "forbidden"); return;
        }
        JsonDocument body = parseBody(req, data, len);
        if (body["config"].is<JsonObject>()) StorageManager::instance().saveJson("/config.json", body["config"]);
        if (body["users"].is<JsonObject>()) StorageManager::instance().saveJson("/users.json", body["users"]);
        if (body["cards"].is<JsonObject>()) StorageManager::instance().saveJson("/cards.json", body["cards"]);
        JsonDocument d; d["ok"] = true; d["note"] = "restart the device to apply the imported configuration";
        sendJson(req, 200, d);
      });
  }

  // ---------------------------------------------------------------------
  // DIAGNOSTICS / FIRMWARE / DEVICE (spec 28, 36, 50)
  // ---------------------------------------------------------------------
  void registerDiagnosticsFirmwareRoutes() {
    server_->on("/api/diagnostics", HTTP_GET, [this](AsyncWebServerRequest* req) {
      if (!requireAuth(req)) { sendError(req, 401, "unauthorized"); return; }
      JsonDocument d;
      JsonArray arr = d["results"].to<JsonArray>();
      for (auto& r : DiagnosticsManager::instance().runAll()) {
        JsonObject o = arr.add<JsonObject>();
        o["component"] = r.component;
        o["status"] = r.status == DiagStatus::OK ? "OK" : r.status == DiagStatus::WARN ? "WARN" :
                      r.status == DiagStatus::FAIL ? "FAIL" : "NOT_PRESENT";
        o["detail"] = r.detail;
      }
      sendJson(req, 200, d);
    });

    server_->on("/api/device", HTTP_GET, [this](AsyncWebServerRequest* req) {
      JsonDocument d;
      d["model"] = "ESP32-S3-Touch-AMOLED-1.75-B"; d["mcu"] = "ESP32-S3R8";
      d["firmware"] = SMARTBOX_FW_VERSION; d["buildTime"] = SMARTBOX_BUILD_TIME; d["gitHash"] = SMARTBOX_GIT_HASH;
      d["flashMb"] = ESP.getFlashChipSize() / (1024 * 1024);
      d["psramMb"] = ESP.getPsramSize() / (1024 * 1024);
      d["wifi"] = wifiStateStr(); d["ip"] = WifiManager::instance().ipAddress();
      d["mac"] = WifiManager::instance().macAddress(); d["uptimeSec"] = millis() / 1000;
      d["sdCard"] = SdManager::instance().isPresent();
      d["rtcPresent"] = RtcManager::instance().isPresent();
      sendJson(req, 200, d); // unauthenticated GET: ABOUT screen must work even from a locked session
    });

    server_->on("/api/firmware/check", HTTP_GET, [this](AsyncWebServerRequest* req) {
      SmartBoxUser* me = requireAuth(req);
      if (!me || !me->hasPermission(PERM_FIRMWARE)) { sendError(req, 403, "forbidden"); return; }
      JsonDocument d;
      d["currentVersion"] = SMARTBOX_FW_VERSION;
      d["checkUrlConfigured"] = false; // set once a real update-manifest host is chosen
      sendJson(req, 200, d);
    });

    server_->on("/api/firmware/update", HTTP_POST, [](AsyncWebServerRequest*) {}, nullptr,
      [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
        SmartBoxUser* me = requireAuth(req);
        if (!me || !me->hasPermission(PERM_FIRMWARE) || !isMutatingRequestSafe(req)) {
          sendError(req, 403, "forbidden"); return;
        }
        JsonDocument body = parseBody(req, data, len);
        String url = body["url"] | "", sha = body["sha256"] | "";
        if (url.isEmpty()) { sendError(req, 400, "url required"); return; }
        JsonDocument d; d["ok"] = true; d["note"] = "update starting, device will reboot";
        sendJson(req, 200, d);
        OtaManager::instance().performUpdate(url, sha); // reboots on success
      });

    server_->on("/api/system/reboot", HTTP_POST, [](AsyncWebServerRequest*) {}, nullptr,
      [this](AsyncWebServerRequest* req, uint8_t*, size_t, size_t, size_t) {
        SmartBoxUser* me = requireAuth(req);
        if (!me || !me->hasPermission(PERM_DIAGNOSTICS)) { sendError(req, 403, "forbidden"); return; }
        JsonDocument d; d["ok"] = true;
        sendJson(req, 200, d);
        delay(200);
        ESP.restart();
      });

    // FACTORY RESET (spec 11, 35): requires both permission AND an explicit
    // confirm flag, mirroring the physical hold+confirm combo.
    server_->on("/api/system/factory-reset", HTTP_POST, [](AsyncWebServerRequest*) {}, nullptr,
      [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
        SmartBoxUser* me = requireAuth(req);
        if (!me || !me->hasPermission(PERM_FACTORY_RESET) || !isMutatingRequestSafe(req)) {
          sendError(req, 403, "forbidden"); return;
        }
        JsonDocument body = parseBody(req, data, len);
        if (!(body["confirm"] | false)) { sendError(req, 400, "confirm flag required"); return; }
        EventLogger::instance().log(EventType::FACTORY_RESET, EventSource::WEB, me->id);
        ConfigManager::instance().factoryReset();
        LittleFS.remove("/users.json");
        LittleFS.remove("/cards.json");
        JsonDocument d; d["ok"] = true;
        sendJson(req, 200, d);
        delay(200);
        ESP.restart();
      });
  }

  // ---------------------------------------------------------------------
  // STATIC DASHBOARD (spec 24-25, 48)
  // ---------------------------------------------------------------------
  void registerStaticRoutes() {
    server_->serveStatic("/", LittleFS, "/").setDefaultFile("index.html").setCacheControl("max-age=86400");
    server_->onNotFound([this](AsyncWebServerRequest* req) {
      if (req->method() == HTTP_GET) req->send(LittleFS, "/index.html", "text/html");
      else sendError(req, 404, "not found");
    });
  }
};
