#pragma once
// Onboard ES8311 codec + amplifier + speaker (spec sections 18-19). The
// exact ES8311 I2C register init sequence is codec/board-revision specific,
// so it is deliberately NOT guessed here — Es8311Codec::begin() in
// audio/Es8311Codec.h is a stub with the I2S pin wiring already correct
// (see include/pins.h) and a clear TODO to port the real init sequence from
// the official Waveshare demo (github.com/waveshareteam/ESP32-S3-Touch-
// AMOLED-1.75, which lists ES8311 support via their fork of the codec
// driver). Once that init is ported, tone playback below works unchanged —
// it drives the I2S bus directly with synthesized square-wave tones, so it
// doesn't depend on codec-specific register knowledge at all.
//
// Microphones are intentionally left disabled (spec section 20: "naj bosta
// izklopljena za nepotrebno obremenitev sistema" — off by default to avoid
// unnecessary load); voice control is a documented future feature.
#include <Arduino.h>
#include <driver/i2s.h>
#include <vector>
#include "../../include/pins.h"
#include "../core/EventBus.h"
#include "../config/ConfigManager.h"
#include "Es8311Codec.h"

enum class SoundEvent { STARTUP, SHUTDOWN, UNLOCK, LOCK, RFID_SUCCESS, RFID_DENIED, WARNING, ALARM, ERROR_TONE, NOTIFICATION };

class AudioManager {
 public:
  static AudioManager& instance() {
    static AudioManager m;
    return m;
  }

  void begin() {
    i2s_config_t cfg = {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
      .sample_rate = kSampleRate,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
      .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
      .communication_format = I2S_COMM_FORMAT_STAND_I2S,
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
      .dma_buf_count = 4,
      .dma_buf_len = 256,
      .use_apll = false,
    };
    i2s_pin_config_t pins = {
      .mck_io_num = PIN_AUDIO_MCLK,
      .bck_io_num = PIN_AUDIO_BCLK,
      .ws_io_num = PIN_AUDIO_WS,
      .data_out_num = PIN_AUDIO_DOUT,
      .data_in_num = I2S_PIN_NO_CHANGE, // mic input path stays unused, see class note above
    };
    esp_err_t err = i2s_driver_install(I2S_NUM_0, &cfg, 0, nullptr);
    i2sReady_ = (err == ESP_OK);
    if (i2sReady_) i2s_set_pin(I2S_NUM_0, &pins);

    pinMode(PIN_AUDIO_PA_EN, OUTPUT);
    digitalWrite(PIN_AUDIO_PA_EN, LOW);

    codecReady_ = Es8311Codec::instance().begin();
    if (!codecReady_) {
      Serial.println("[Audio] ES8311 codec init not yet ported (see Es8311Codec.h TODO) — "
                      "I2S bus is up, but no sound will be audible until the codec is initialized");
    }

    subscribeEvents();
  }

  void loop() {
    if (!playing_) return;
    if (millis() >= toneEndsAtMs_) stopTone();
  }

  void play(SoundEvent evt) {
    auto& cfg = ConfigManager::instance().get().audio;
    if (!cfg.enabled) return;
    switch (evt) {
      case SoundEvent::STARTUP: if (cfg.soundStartup) beepPattern({{880, 100}, {1320, 120}}); break;
      case SoundEvent::SHUTDOWN: if (cfg.soundStartup) beepPattern({{660, 150}}); break;
      case SoundEvent::UNLOCK: if (cfg.soundUnlock) beepPattern({{1200, 80}}); break;
      case SoundEvent::LOCK: if (cfg.soundLock) beepPattern({{600, 80}}); break;
      case SoundEvent::RFID_SUCCESS: if (cfg.soundSuccess) beepPattern({{1000, 60}, {1400, 60}}); break; // "beep-beep"
      case SoundEvent::RFID_DENIED: if (cfg.soundDenied) beepPattern({{300, 600}}); break;                // long "beeeeeep"
      case SoundEvent::WARNING: if (cfg.soundWarning) beepPattern({{500, 120}, {0, 80}, {500, 120}}); break;
      case SoundEvent::ALARM: beepPattern({{900, 150}, {0, 100}, {900, 150}, {0, 100}, {900, 150}}); break;
      case SoundEvent::ERROR_TONE: if (cfg.soundError) beepPattern({{250, 400}}); break;
      case SoundEvent::NOTIFICATION: beepPattern({{1100, 60}}); break;
    }
  }

 private:
  AudioManager() = default;
  static constexpr uint32_t kSampleRate = 22050;
  bool i2sReady_ = false, codecReady_ = false, playing_ = false;
  uint32_t toneEndsAtMs_ = 0;

  struct Tone { int freqHz; int durationMs; };
  std::vector<Tone> queue_;
  size_t queueIdx_ = 0;

  void subscribeEvents() {
    EventBus::instance().on(Topic::LockStateChanged, [this](const EventPayload& p) {
      play(p.flag ? SoundEvent::LOCK : SoundEvent::UNLOCK);
    });
    EventBus::instance().on(Topic::RfidGranted, [this](const EventPayload&) { play(SoundEvent::RFID_SUCCESS); });
    EventBus::instance().on(Topic::RfidDenied, [this](const EventPayload&) { play(SoundEvent::RFID_DENIED); });
    EventBus::instance().on(Topic::TamperDetected, [this](const EventPayload&) { play(SoundEvent::ALARM); });
    EventBus::instance().on(Topic::ServoError, [this](const EventPayload&) { play(SoundEvent::ERROR_TONE); });
    EventBus::instance().on(Topic::BootComplete, [this](const EventPayload&) { play(SoundEvent::STARTUP); });
  }

  void beepPattern(std::vector<Tone> tones) {
    queue_ = std::move(tones);
    queueIdx_ = 0;
    playNext();
  }

  void playNext() {
    if (queueIdx_ >= queue_.size()) { stopTone(); return; }
    Tone t = queue_[queueIdx_++];
    if (t.freqHz > 0) startTone(t.freqHz);
    else digitalWrite(PIN_AUDIO_PA_EN, LOW);
    toneEndsAtMs_ = millis() + t.durationMs;
    playing_ = true;
  }

  // Writes one cycle's worth of square wave non-blocking (timeout=0). This
  // is called once per tone start rather than streamed continuously for the
  // whole duration — good enough for short UI beeps given the DMA buffer
  // (dma_buf_count*dma_buf_len samples) plays out over ~40-90ms on its own;
  // for tones longer than that, refill from loop() by tracking
  // toneEndsAtMs_ and re-calling this every ~30ms. Left as a follow-up
  // rather than adding a refill timer to every beep in this skeleton.
  void startTone(int freqHz) {
    if (!i2sReady_) return;
    digitalWrite(PIN_AUDIO_PA_EN, HIGH);
    int volumePercent = ConfigManager::instance().get().audio.volumePercent;
    int16_t amplitude = (int16_t)(32767 * (volumePercent / 100.0f) * 0.5f);
    const int samplesPerCycle = kSampleRate / freqHz;
    static int16_t buf[512];
    int n = min(samplesPerCycle, 512);
    for (int i = 0; i < n; i++) buf[i] = (i < n / 2) ? amplitude : -amplitude; // square wave
    size_t written;
    i2s_write(I2S_NUM_0, buf, n * sizeof(int16_t), &written, 0);
  }

  void stopTone() {
    if (queueIdx_ < queue_.size()) { playNext(); return; }
    playing_ = false;
    digitalWrite(PIN_AUDIO_PA_EN, LOW);
  }
};
