#pragma once
// OTA firmware update over HTTP(S) (spec section 36). Config (LittleFS) is
// untouched by an OTA write since it lives in its own partition (see
// partitions.csv) — the app image alone is replaced. Integrity is checked
// via a SHA-256 digest supplied alongside the firmware (in the version
// manifest JSON), compared against a running digest of the downloaded
// bytes before Update.end() commits anything. ESP32 Arduino's dual-OTA
// partitioning (app0/app1) plus esp_ota_mark_app_valid_cancel_rollback
// gives us the "safe recovery" spec asks for: if the new image doesn't
// call markBootSuccessful() within kBootValidationWindowMs of booting,
// the bootloader falls back to the previous slot on the next reset.
#include <Arduino.h>
#include <Update.h>
#include <HTTPClient.h>
#include <mbedtls/sha256.h>
#include <esp_ota_ops.h>
#include "../core/EventBus.h"
#include "../events/EventLogger.h"

enum class OtaState { IDLE, CHECKING, DOWNLOADING, VERIFYING, DONE, ERROR };

class OtaManager {
 public:
  static OtaManager& instance() {
    static OtaManager m;
    return m;
  }

  void begin() {
    // Give the new image a grace period to prove itself (web server up,
    // no crash loop) before we consider it "valid" and cancel rollback.
    bootedAtMs_ = millis();
  }

  void loop() {
    if (!bootValidated_ && millis() - bootedAtMs_ > kBootValidationWindowMs) {
      markBootSuccessful();
    }
  }

  OtaState state() const { return state_; }
  String currentVersion() const { return SMARTBOX_FW_VERSION; }
  String lastError() const { return lastError_; }

  // manifestUrl should point to a small JSON document: {"version":"1.2.0","url":"https://.../firmware.bin","sha256":"..."}
  bool checkForUpdate(const String& manifestUrl, String& outVersion, String& outUrl, String& outSha256) {
    state_ = OtaState::CHECKING;
    HTTPClient http;
    http.begin(manifestUrl);
    int code = http.GET();
    if (code != 200) { fail("manifest fetch failed: HTTP " + String(code)); http.end(); return false; }
    String body = http.getString();
    http.end();

    JsonDocument doc;
    if (deserializeJson(doc, body)) { fail("manifest parse error"); return false; }
    outVersion = doc["version"] | "";
    outUrl = doc["url"] | "";
    outSha256 = doc["sha256"] | "";
    state_ = OtaState::IDLE;
    return outVersion.length() && outUrl.length();
  }

  bool performUpdate(const String& firmwareUrl, const String& expectedSha256Hex) {
    EventBus::instance().emit(Topic::OtaStarted);
    EventLogger::instance().log(EventType::FIRMWARE_UPDATE, EventSource::SYSTEM, "", "", "STARTED");
    state_ = OtaState::DOWNLOADING;

    HTTPClient http;
    http.begin(firmwareUrl);
    int code = http.GET();
    if (code != 200) { fail("download failed: HTTP " + String(code)); http.end(); return false; }

    int total = http.getSize();
    if (!Update.begin(total > 0 ? total : UPDATE_SIZE_UNKNOWN)) {
      fail("Update.begin failed: " + String(Update.errorString()));
      http.end();
      return false;
    }

    mbedtls_sha256_context sha;
    mbedtls_sha256_init(&sha);
    mbedtls_sha256_starts(&sha, 0);

    WiFiClient* stream = http.getStreamPtr();
    uint8_t buf[1024];
    size_t written = 0;
    while (http.connected() && (total < 0 || (int)written < total)) {
      size_t avail = stream->available();
      if (!avail) { delay(1); continue; }
      size_t n = stream->readBytes(buf, min(avail, sizeof(buf)));
      if (n == 0) break;
      Update.write(buf, n);
      mbedtls_sha256_update(&sha, buf, n);
      written += n;
    }
    http.end();

    uint8_t digest[32];
    mbedtls_sha256_finish(&sha, digest);
    mbedtls_sha256_free(&sha);

    state_ = OtaState::VERIFYING;
    String digestHex = toHex(digest, 32);
    if (expectedSha256Hex.length() && !digestHex.equalsIgnoreCase(expectedSha256Hex)) {
      Update.abort();
      fail("checksum mismatch — refusing to install");
      return false;
    }

    if (!Update.end(true)) {
      fail("Update.end failed: " + String(Update.errorString()));
      return false;
    }

    state_ = OtaState::DONE;
    EventBus::instance().emit(Topic::OtaFinished, EventPayload("ok"));
    EventLogger::instance().log(EventType::FIRMWARE_UPDATE, EventSource::SYSTEM, "", "", "OK");
    ESP.restart();
    return true; // unreachable, kept for API clarity
  }

  void markBootSuccessful() {
    bootValidated_ = true;
    esp_ota_mark_app_valid_cancel_rollback();
  }

 private:
  OtaManager() = default;
  OtaState state_ = OtaState::IDLE;
  String lastError_;
  uint32_t bootedAtMs_ = 0;
  bool bootValidated_ = false;
  static constexpr uint32_t kBootValidationWindowMs = 60000; // 1 min of healthy uptime before we trust the image

  void fail(const String& msg) {
    state_ = OtaState::ERROR;
    lastError_ = msg;
    Serial.printf("[OTA] %s\n", msg.c_str());
    EventLogger::instance().log(EventType::FIRMWARE_UPDATE, EventSource::SYSTEM, "", "", "ERROR", msg);
  }

  String toHex(const uint8_t* data, size_t len) {
    String s;
    for (size_t i = 0; i < len; i++) { if (data[i] < 0x10) s += "0"; s += String(data[i], HEX); }
    return s;
  }
};
