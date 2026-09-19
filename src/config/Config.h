#pragma once
// Plain data model for everything persisted in /config.json (spec section 60:
// "Konfiguracija mora biti trajno shranjena"). Kept as simple structs so
// ConfigManager, the web REST API and the AMOLED settings screens all share
// one definition instead of three drifting copies.
#include <Arduino.h>

struct ServoConfig {
  bool    enabled       = false;   // stays false (safe/no PWM output) until confirmed via web UI
  int     gpio          = -1;
  int     lockedAngle   = 20;
  int     unlockedAngle = 105;
  int     speedDegPerSec = 180;
  int     delayMs       = 0;
  bool    invert        = false;
};

struct LidConfig {
  bool enabled     = false;
  int  gpio        = -1;
  bool inverted    = false;
  int  debounceMs  = 50;
  bool autoLock    = false;
  int  autoLockDelaySec = 2;       // 0-30s per spec 17
  bool warningOnOpenWhileLocked = true;
  bool logging     = true;
};

struct ButtonConfig {
  int  gpio           = -1;
  int  longPressMs    = 1000;
  int  doublePressGapMs = 350;
  // Action identifiers — see buttons/ButtonActions.h for the enum + dispatch.
  String shortAction  = "SHOW_STATUS";
  String longAction   = "GENERATE_SECURITY_CODE";
  String doubleAction = "OPEN_QUICK_MENU";
};

struct WifiConfig {
  String ssid;
  String password;
  String hostname   = "smartbox";
  String deviceName = "SMART BOX";
  String timezone   = "Europe/Ljubljana";
  String ntpServer  = "pool.ntp.org";
  bool   use24h     = true;
};

struct SecurityConfig {
  int  sessionTimeoutSec   = 8 * 60;  // spec 12
  int  securityCodeTtlMs   = 1000;    // "velja približno 1 sekundo"
  int  rfidReadTimeoutMs   = 5000;
  int  unlockDurationMs    = 4000;    // how long doors stay logically "unlocked"/open-permitted
  int  rfidCooldownMs      = 1500;
  int  maxFailedAttempts   = 5;
  int  accessDeniedDelayMs = 1500;
  bool loggingEnabled      = true;
};

struct AudioConfig {
  bool enabled       = true;
  int  volumePercent  = 70;
  bool soundSuccess   = true;
  bool soundDenied    = true;
  bool soundLock      = true;
  bool soundUnlock    = true;
  bool soundWarning   = true;
  bool soundStartup   = true;
  bool soundError     = true;
};

struct AppearanceConfig {
  // Web dashboard theme (AMOLED-specific look is in DisplayConfig below).
  String theme = "auto";           // light|dark|auto|custom
  String colorPrimary   = "#2563eb";
  String colorSecondary = "#7c3aed";
  String colorBackground = "#0b0f19";
  String colorCard      = "#141a29";
  String colorText      = "#e6e9f0";
  String colorMuted     = "#8890a3";
  String colorSuccess   = "#22c55e";
  String colorWarning   = "#f59e0b";
  String colorDanger    = "#ef4444";
  String colorInfo      = "#38bdf8";
  String colorLocked    = "#ef4444";
  String colorUnlocked  = "#22c55e";
  int    borderRadius   = 14;
  bool   animationsEnabled = true;
  String dashboardDensity = "comfortable"; // comfortable|compact
  int    fontSizePercent = 100;
};

struct DisplayConfig {
  int  brightnessPercent   = 80;
  int  screenTimeoutSec    = 30;
  int  rotationDeg         = 0;
  String defaultScreen     = "HOME";
  bool  animationsEnabled  = true;
  int   notificationDurationMs = 2500;
  int   touchSensitivity   = 3; // 1-5, driver-dependent
};

struct LocaleConfig {
  String language = "sl";  // sl|en|de|it|hr|sr (spec 49)
  String dateFormat = "DD.MM.YYYY";
};

struct SmartBoxConfig {
  ServoConfig     servo1;
  ServoConfig     servo2;
  LidConfig       lid;
  ButtonConfig    button1;
  ButtonConfig    button2;
  WifiConfig      wifi;
  SecurityConfig  security;
  AudioConfig     audio;
  AppearanceConfig appearance;
  DisplayConfig   display;
  LocaleConfig    locale;
  bool            provisioned = false; // false until first-boot WIFI SETUP completes or is skipped
};
