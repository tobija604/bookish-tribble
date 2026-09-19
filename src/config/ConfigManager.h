#pragma once
#include "Config.h"
#include "../core/EventBus.h"
#include "../storage/StorageManager.h"

// Loads/saves the single /config.json document and hands out a reference to
// the live config struct. Every module reads through this at begin() and
// again whenever Topic::ConfigChanged fires (emitted by save()) instead of
// caching values, so a change made in the web UI takes effect without a
// reboot wherever that's mechanically possible (servo angles, lid delay,
// display brightness, ...). A few things genuinely need a reboot (WiFi
// credentials, hostname) — those screens say so explicitly in the web UI.
class ConfigManager {
 public:
  static ConfigManager& instance() {
    static ConfigManager mgr;
    return mgr;
  }

  void begin() {
    JsonDocument doc;
    if (StorageManager::instance().loadJson(kPath, doc)) {
      fromJson(doc);
      Serial.println("[Config] loaded from flash");
    } else {
      Serial.println("[Config] no config on flash, using defaults");
      cfg_ = SmartBoxConfig(); // struct defaults from Config.h
    }
  }

  SmartBoxConfig& get() { return cfg_; }
  const SmartBoxConfig& get() const { return cfg_; }

  bool save() {
    JsonDocument doc;
    toJson(doc);
    bool ok = StorageManager::instance().saveJson(kPath, doc);
    if (ok) EventBus::instance().emit(Topic::ConfigChanged);
    return ok;
  }

  void factoryReset() {
    cfg_ = SmartBoxConfig();
    save();
  }

 private:
  ConfigManager() = default;
  static constexpr const char* kPath = "/config.json";
  SmartBoxConfig cfg_;

  void toJson(JsonDocument& d) const {
    d["provisioned"] = cfg_.provisioned;

    auto srv = [&](const char* key, const ServoConfig& s) {
      JsonObject o = d[key].to<JsonObject>();
      o["enabled"] = s.enabled; o["gpio"] = s.gpio;
      o["lockedAngle"] = s.lockedAngle; o["unlockedAngle"] = s.unlockedAngle;
      o["speedDegPerSec"] = s.speedDegPerSec; o["delayMs"] = s.delayMs; o["invert"] = s.invert;
    };
    srv("servo1", cfg_.servo1);
    srv("servo2", cfg_.servo2);

    JsonObject lid = d["lid"].to<JsonObject>();
    lid["enabled"] = cfg_.lid.enabled; lid["gpio"] = cfg_.lid.gpio;
    lid["inverted"] = cfg_.lid.inverted; lid["debounceMs"] = cfg_.lid.debounceMs;
    lid["autoLock"] = cfg_.lid.autoLock; lid["autoLockDelaySec"] = cfg_.lid.autoLockDelaySec;
    lid["warningOnOpenWhileLocked"] = cfg_.lid.warningOnOpenWhileLocked;
    lid["logging"] = cfg_.lid.logging;

    auto btn = [&](const char* key, const ButtonConfig& b) {
      JsonObject o = d[key].to<JsonObject>();
      o["gpio"] = b.gpio; o["longPressMs"] = b.longPressMs;
      o["doublePressGapMs"] = b.doublePressGapMs;
      o["shortAction"] = b.shortAction; o["longAction"] = b.longAction; o["doubleAction"] = b.doubleAction;
    };
    btn("button1", cfg_.button1);
    btn("button2", cfg_.button2);

    JsonObject wifi = d["wifi"].to<JsonObject>();
    wifi["ssid"] = cfg_.wifi.ssid; wifi["password"] = cfg_.wifi.password;
    wifi["hostname"] = cfg_.wifi.hostname; wifi["deviceName"] = cfg_.wifi.deviceName;
    wifi["timezone"] = cfg_.wifi.timezone; wifi["ntpServer"] = cfg_.wifi.ntpServer;
    wifi["use24h"] = cfg_.wifi.use24h;

    JsonObject sec = d["security"].to<JsonObject>();
    sec["sessionTimeoutSec"] = cfg_.security.sessionTimeoutSec;
    sec["securityCodeTtlMs"] = cfg_.security.securityCodeTtlMs;
    sec["rfidReadTimeoutMs"] = cfg_.security.rfidReadTimeoutMs;
    sec["unlockDurationMs"] = cfg_.security.unlockDurationMs;
    sec["rfidCooldownMs"] = cfg_.security.rfidCooldownMs;
    sec["maxFailedAttempts"] = cfg_.security.maxFailedAttempts;
    sec["accessDeniedDelayMs"] = cfg_.security.accessDeniedDelayMs;
    sec["loggingEnabled"] = cfg_.security.loggingEnabled;

    JsonObject audio = d["audio"].to<JsonObject>();
    audio["enabled"] = cfg_.audio.enabled; audio["volumePercent"] = cfg_.audio.volumePercent;
    audio["soundSuccess"] = cfg_.audio.soundSuccess; audio["soundDenied"] = cfg_.audio.soundDenied;
    audio["soundLock"] = cfg_.audio.soundLock; audio["soundUnlock"] = cfg_.audio.soundUnlock;
    audio["soundWarning"] = cfg_.audio.soundWarning; audio["soundStartup"] = cfg_.audio.soundStartup;
    audio["soundError"] = cfg_.audio.soundError;

    JsonObject app = d["appearance"].to<JsonObject>();
    app["theme"] = cfg_.appearance.theme;
    app["colorPrimary"] = cfg_.appearance.colorPrimary;
    app["colorSecondary"] = cfg_.appearance.colorSecondary;
    app["colorBackground"] = cfg_.appearance.colorBackground;
    app["colorCard"] = cfg_.appearance.colorCard;
    app["colorText"] = cfg_.appearance.colorText;
    app["colorMuted"] = cfg_.appearance.colorMuted;
    app["colorSuccess"] = cfg_.appearance.colorSuccess;
    app["colorWarning"] = cfg_.appearance.colorWarning;
    app["colorDanger"] = cfg_.appearance.colorDanger;
    app["colorInfo"] = cfg_.appearance.colorInfo;
    app["colorLocked"] = cfg_.appearance.colorLocked;
    app["colorUnlocked"] = cfg_.appearance.colorUnlocked;
    app["borderRadius"] = cfg_.appearance.borderRadius;
    app["animationsEnabled"] = cfg_.appearance.animationsEnabled;
    app["dashboardDensity"] = cfg_.appearance.dashboardDensity;
    app["fontSizePercent"] = cfg_.appearance.fontSizePercent;

    JsonObject disp = d["display"].to<JsonObject>();
    disp["brightnessPercent"] = cfg_.display.brightnessPercent;
    disp["screenTimeoutSec"] = cfg_.display.screenTimeoutSec;
    disp["rotationDeg"] = cfg_.display.rotationDeg;
    disp["defaultScreen"] = cfg_.display.defaultScreen;
    disp["animationsEnabled"] = cfg_.display.animationsEnabled;
    disp["notificationDurationMs"] = cfg_.display.notificationDurationMs;
    disp["touchSensitivity"] = cfg_.display.touchSensitivity;

    JsonObject loc = d["locale"].to<JsonObject>();
    loc["language"] = cfg_.locale.language;
    loc["dateFormat"] = cfg_.locale.dateFormat;
  }

  void fromJson(JsonDocument& d) {
    cfg_ = SmartBoxConfig(); // start from defaults, overlay what's present
    cfg_.provisioned = d["provisioned"] | false;

    auto srv = [&](const char* key, ServoConfig& s) {
      if (!d[key].is<JsonObject>()) return;
      JsonObject o = d[key];
      s.enabled = o["enabled"] | s.enabled; s.gpio = o["gpio"] | s.gpio;
      s.lockedAngle = o["lockedAngle"] | s.lockedAngle;
      s.unlockedAngle = o["unlockedAngle"] | s.unlockedAngle;
      s.speedDegPerSec = o["speedDegPerSec"] | s.speedDegPerSec;
      s.delayMs = o["delayMs"] | s.delayMs; s.invert = o["invert"] | s.invert;
    };
    srv("servo1", cfg_.servo1);
    srv("servo2", cfg_.servo2);

    if (d["lid"].is<JsonObject>()) {
      JsonObject o = d["lid"];
      cfg_.lid.enabled = o["enabled"] | cfg_.lid.enabled;
      cfg_.lid.gpio = o["gpio"] | cfg_.lid.gpio;
      cfg_.lid.inverted = o["inverted"] | cfg_.lid.inverted;
      cfg_.lid.debounceMs = o["debounceMs"] | cfg_.lid.debounceMs;
      cfg_.lid.autoLock = o["autoLock"] | cfg_.lid.autoLock;
      cfg_.lid.autoLockDelaySec = o["autoLockDelaySec"] | cfg_.lid.autoLockDelaySec;
      cfg_.lid.warningOnOpenWhileLocked = o["warningOnOpenWhileLocked"] | cfg_.lid.warningOnOpenWhileLocked;
      cfg_.lid.logging = o["logging"] | cfg_.lid.logging;
    }

    auto btn = [&](const char* key, ButtonConfig& b) {
      if (!d[key].is<JsonObject>()) return;
      JsonObject o = d[key];
      b.gpio = o["gpio"] | b.gpio; b.longPressMs = o["longPressMs"] | b.longPressMs;
      b.doublePressGapMs = o["doublePressGapMs"] | b.doublePressGapMs;
      if (o["shortAction"].is<const char*>()) b.shortAction = o["shortAction"].as<String>();
      if (o["longAction"].is<const char*>()) b.longAction = o["longAction"].as<String>();
      if (o["doubleAction"].is<const char*>()) b.doubleAction = o["doubleAction"].as<String>();
    };
    btn("button1", cfg_.button1);
    btn("button2", cfg_.button2);

    if (d["wifi"].is<JsonObject>()) {
      JsonObject o = d["wifi"];
      cfg_.wifi.ssid = o["ssid"] | cfg_.wifi.ssid;
      cfg_.wifi.password = o["password"] | cfg_.wifi.password;
      cfg_.wifi.hostname = o["hostname"] | cfg_.wifi.hostname;
      cfg_.wifi.deviceName = o["deviceName"] | cfg_.wifi.deviceName;
      cfg_.wifi.timezone = o["timezone"] | cfg_.wifi.timezone;
      cfg_.wifi.ntpServer = o["ntpServer"] | cfg_.wifi.ntpServer;
      cfg_.wifi.use24h = o["use24h"] | cfg_.wifi.use24h;
    }

    if (d["security"].is<JsonObject>()) {
      JsonObject o = d["security"];
      cfg_.security.sessionTimeoutSec = o["sessionTimeoutSec"] | cfg_.security.sessionTimeoutSec;
      cfg_.security.securityCodeTtlMs = o["securityCodeTtlMs"] | cfg_.security.securityCodeTtlMs;
      cfg_.security.rfidReadTimeoutMs = o["rfidReadTimeoutMs"] | cfg_.security.rfidReadTimeoutMs;
      cfg_.security.unlockDurationMs = o["unlockDurationMs"] | cfg_.security.unlockDurationMs;
      cfg_.security.rfidCooldownMs = o["rfidCooldownMs"] | cfg_.security.rfidCooldownMs;
      cfg_.security.maxFailedAttempts = o["maxFailedAttempts"] | cfg_.security.maxFailedAttempts;
      cfg_.security.accessDeniedDelayMs = o["accessDeniedDelayMs"] | cfg_.security.accessDeniedDelayMs;
      cfg_.security.loggingEnabled = o["loggingEnabled"] | cfg_.security.loggingEnabled;
    }

    if (d["audio"].is<JsonObject>()) {
      JsonObject o = d["audio"];
      cfg_.audio.enabled = o["enabled"] | cfg_.audio.enabled;
      cfg_.audio.volumePercent = o["volumePercent"] | cfg_.audio.volumePercent;
      cfg_.audio.soundSuccess = o["soundSuccess"] | cfg_.audio.soundSuccess;
      cfg_.audio.soundDenied = o["soundDenied"] | cfg_.audio.soundDenied;
      cfg_.audio.soundLock = o["soundLock"] | cfg_.audio.soundLock;
      cfg_.audio.soundUnlock = o["soundUnlock"] | cfg_.audio.soundUnlock;
      cfg_.audio.soundWarning = o["soundWarning"] | cfg_.audio.soundWarning;
      cfg_.audio.soundStartup = o["soundStartup"] | cfg_.audio.soundStartup;
      cfg_.audio.soundError = o["soundError"] | cfg_.audio.soundError;
    }

    if (d["appearance"].is<JsonObject>()) {
      JsonObject o = d["appearance"];
      cfg_.appearance.theme = o["theme"] | cfg_.appearance.theme;
      cfg_.appearance.colorPrimary = o["colorPrimary"] | cfg_.appearance.colorPrimary;
      cfg_.appearance.colorSecondary = o["colorSecondary"] | cfg_.appearance.colorSecondary;
      cfg_.appearance.colorBackground = o["colorBackground"] | cfg_.appearance.colorBackground;
      cfg_.appearance.colorCard = o["colorCard"] | cfg_.appearance.colorCard;
      cfg_.appearance.colorText = o["colorText"] | cfg_.appearance.colorText;
      cfg_.appearance.colorMuted = o["colorMuted"] | cfg_.appearance.colorMuted;
      cfg_.appearance.colorSuccess = o["colorSuccess"] | cfg_.appearance.colorSuccess;
      cfg_.appearance.colorWarning = o["colorWarning"] | cfg_.appearance.colorWarning;
      cfg_.appearance.colorDanger = o["colorDanger"] | cfg_.appearance.colorDanger;
      cfg_.appearance.colorInfo = o["colorInfo"] | cfg_.appearance.colorInfo;
      cfg_.appearance.colorLocked = o["colorLocked"] | cfg_.appearance.colorLocked;
      cfg_.appearance.colorUnlocked = o["colorUnlocked"] | cfg_.appearance.colorUnlocked;
      cfg_.appearance.borderRadius = o["borderRadius"] | cfg_.appearance.borderRadius;
      cfg_.appearance.animationsEnabled = o["animationsEnabled"] | cfg_.appearance.animationsEnabled;
      cfg_.appearance.dashboardDensity = o["dashboardDensity"] | cfg_.appearance.dashboardDensity;
      cfg_.appearance.fontSizePercent = o["fontSizePercent"] | cfg_.appearance.fontSizePercent;
    }

    if (d["display"].is<JsonObject>()) {
      JsonObject o = d["display"];
      cfg_.display.brightnessPercent = o["brightnessPercent"] | cfg_.display.brightnessPercent;
      cfg_.display.screenTimeoutSec = o["screenTimeoutSec"] | cfg_.display.screenTimeoutSec;
      cfg_.display.rotationDeg = o["rotationDeg"] | cfg_.display.rotationDeg;
      cfg_.display.defaultScreen = o["defaultScreen"] | cfg_.display.defaultScreen;
      cfg_.display.animationsEnabled = o["animationsEnabled"] | cfg_.display.animationsEnabled;
      cfg_.display.notificationDurationMs = o["notificationDurationMs"] | cfg_.display.notificationDurationMs;
      cfg_.display.touchSensitivity = o["touchSensitivity"] | cfg_.display.touchSensitivity;
    }

    if (d["locale"].is<JsonObject>()) {
      JsonObject o = d["locale"];
      cfg_.locale.language = o["language"] | cfg_.locale.language;
      cfg_.locale.dateFormat = o["dateFormat"] | cfg_.locale.dateFormat;
    }
  }
};
