#pragma once
// AMOLED + LVGL bring-up (spec sections 2-3, 27). Uses moononournation's
// Arduino_GFX (Arduino_CO5300 driver class over a QSPI bus) as the LVGL
// display flush target, and Ft3168Touch as the LVGL input device. Screen
// widgets themselves live in display/screens/*.h; this class only owns the
// hardware bring-up, the LVGL tick/task-handler pump, brightness/timeout,
// and which screen is currently active.
#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <lvgl.h>
#include "../../include/pins.h"
#include "../core/Module.h"
#include "../core/EventBus.h"
#include "../config/ConfigManager.h"
#include "../touch/Ft3168Touch.h"

class DisplayManager : public IModule {
 public:
  static DisplayManager& instance() {
    static DisplayManager m;
    return m;
  }

  void begin() override {
    bus_ = new Arduino_ESP32QSPI(PIN_LCD_CS, PIN_LCD_SCLK, PIN_LCD_QSPI_D0, PIN_LCD_QSPI_D1,
                                  PIN_LCD_QSPI_D2, PIN_LCD_QSPI_D3);
    // CO5300, 466x466 round AMOLED. Confirm col/row offsets & IPS flag
    // against the Waveshare demo's panel init if the image is offset/mirrored.
    gfx_ = new Arduino_CO5300(bus_, PIN_LCD_RESET, 0 /*rotation*/, false /*IPS*/, 466, 466,
                                0, 0, 0, 0);
    gfx_->begin();
    gfx_->fillScreen(BLACK);

    Ft3168Touch::instance().begin();

    lv_init();
    initLvglBuffers();
    registerLvglDisplay();
    registerLvglInput();

    setBrightnessPercent(ConfigManager::instance().get().display.brightnessPercent);
    lastActivityMs_ = millis();

    EventBus::instance().on(Topic::ConfigChanged, [this](const EventPayload&) {
      setBrightnessPercent(ConfigManager::instance().get().display.brightnessPercent);
    });
  }

  void loop() override {
    lv_tick_inc(5);
    lv_timer_handler();

    // Screen timeout / dim-on-idle (spec 27 "screen timeout").
    int timeoutSec = ConfigManager::instance().get().display.screenTimeoutSec;
    if (timeoutSec > 0 && !dimmed_ && millis() - lastActivityMs_ > (uint32_t)timeoutSec * 1000UL) {
      dimmed_ = true;
      setBrightnessPercent(5); // dim rather than fully blank, so the LOCKED/UNLOCKED state stays glanceable
    }
  }

  const char* name() const override { return "DisplayManager"; }

  void notifyActivity() {
    lastActivityMs_ = millis();
    if (dimmed_) { dimmed_ = false; setBrightnessPercent(ConfigManager::instance().get().display.brightnessPercent); }
  }

  void setBrightnessPercent(int pct) {
    pct = constrain(pct, 0, 100);
    // Arduino_CO5300 (AMOLED, self-emissive) typically exposes brightness
    // via a display-on command + a backlight-equivalent register rather
    // than a PWM backlight pin — call the GFX driver's brightness setter if
    // the installed Arduino_GFX version exposes one; some builds expose
    // Arduino_GFX::setBrightness(uint8_t) directly.
#if defined(ARDUINO_GFX_HAS_SET_BRIGHTNESS)
    gfx_->setBrightness((uint8_t)(pct * 255 / 100));
#endif
  }

  Arduino_GFX* gfx() { return gfx_; }

 private:
  DisplayManager() = default;
  Arduino_ESP32QSPI* bus_ = nullptr;
  Arduino_CO5300* gfx_ = nullptr;
  lv_disp_draw_buf_t drawBuf_;
  lv_color_t* buf1_ = nullptr;
  lv_disp_drv_t dispDrv_;
  lv_indev_drv_t indevDrv_;
  uint32_t lastActivityMs_ = 0;
  bool dimmed_ = false;

  static constexpr uint16_t kScreenW = 466, kScreenH = 466;

  void initLvglBuffers() {
    // One partial buffer in PSRAM is plenty for a 466x466 panel; a full
    // 466*466*2-byte frame buffer would be ~435KB, comfortably inside the
    // 8MB PSRAM budget if a full-frame buffer is preferred instead — kept
    // partial here to leave more PSRAM headroom for LVGL image caches and
    // the web server's async buffers.
    size_t bufPixels = kScreenW * 40;
    buf1_ = (lv_color_t*)heap_caps_malloc(bufPixels * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    lv_disp_draw_buf_init(&drawBuf_, buf1_, nullptr, bufPixels);
  }

  void registerLvglDisplay() {
    lv_disp_drv_init(&dispDrv_);
    dispDrv_.hor_res = kScreenW;
    dispDrv_.ver_res = kScreenH;
    dispDrv_.flush_cb = &DisplayManager::flushCb;
    dispDrv_.draw_buf = &drawBuf_;
    dispDrv_.user_data = this;
    lv_disp_drv_register(&dispDrv_);
  }

  void registerLvglInput() {
    lv_indev_drv_init(&indevDrv_);
    indevDrv_.type = LV_INDEV_TYPE_POINTER;
    indevDrv_.read_cb = &DisplayManager::touchReadCb;
    lv_indev_drv_register(&indevDrv_);
  }

  static void flushCb(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* colorP) {
    DisplayManager* self = (DisplayManager*)drv->user_data;
    uint32_t w = area->x2 - area->x1 + 1;
    uint32_t h = area->y2 - area->y1 + 1;
    self->gfx_->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t*)colorP, w, h);
    lv_disp_flush_ready(drv);
  }

  static void touchReadCb(lv_indev_drv_t* drv, lv_indev_data_t* data) {
    (void)drv;
    int16_t x, y;
    if (Ft3168Touch::instance().read(x, y)) {
      data->state = LV_INDEV_STATE_PRESSED;
      data->point.x = x;
      data->point.y = y;
      DisplayManager::instance().notifyActivity();
    } else {
      data->state = LV_INDEV_STATE_RELEASED;
    }
  }
};
