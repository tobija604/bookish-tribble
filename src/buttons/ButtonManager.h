#pragma once
// Debounces and classifies presses on BUTTON 1 / BUTTON 2 (spec sections
// 9-11): short press, long press (~1s, configurable), double click (within
// a configurable gap), and the BUTTON 2 factory-reset combo (hold 10s, then
// require AMOLED confirmation — spec section 11). This module only detects
// press patterns and emits EventBus topics; see ButtonActionDispatcher.h for
// what each pattern actually *does*, which is configurable per spec 9.
#include <Arduino.h>
#include "../core/Module.h"
#include "../core/EventBus.h"
#include "../config/ConfigManager.h"
#include "../config/HardwareConfig.h"

class ButtonManager : public IModule {
 public:
  static ButtonManager& instance() {
    static ButtonManager m;
    return m;
  }

  void begin() override {
    auto& cfg = ConfigManager::instance().get();
    if (cfg.button1.gpio >= 0) HardwareConfig::instance().pinMode(cfg.button1.gpio, INPUT_PULLUP);
    if (cfg.button2.gpio >= 0) HardwareConfig::instance().pinMode(cfg.button2.gpio, INPUT_PULLUP);
  }

  void loop() override {
    auto& cfg = ConfigManager::instance().get();
    pollButton(1, cfg.button1);
    pollButton(2, cfg.button2);
  }

  const char* name() const override { return "ButtonManager"; }

 private:
  ButtonManager() = default;

  struct BtnState {
    bool pressed = false;
    uint32_t pressStartMs = 0;
    uint32_t lastReleaseMs = 0;
    bool awaitingSecondClick = false;
    bool longFired = false;
    bool factoryResetArmed = false; // button2 only
    uint32_t pendingShortHeldMs = 0; // reserved for a future "custom duration" action variant (spec 9)
  };
  BtnState b1_, b2_;

  void pollButton(int which, const ButtonConfig& cfg) {
    if (cfg.gpio < 0) return;
    BtnState& st = (which == 1) ? b1_ : b2_;
    bool down = (HardwareConfig::instance().digitalRead(cfg.gpio) == LOW); // active-low, INPUT_PULLUP
    uint32_t now = millis();

    if (down && !st.pressed) {
      st.pressed = true;
      st.pressStartMs = now;
      st.longFired = false;
    } else if (down && st.pressed) {
      uint32_t heldMs = now - st.pressStartMs;
      if (!st.longFired && heldMs >= (uint32_t)cfg.longPressMs) {
        st.longFired = true;
        emitPress(which, "LONG");
      }
      // BUTTON 2 factory-reset combo: hold 10s (spec section 11).
      if (which == 2 && !st.factoryResetArmed && heldMs >= 10000UL) {
        st.factoryResetArmed = true;
        EventBus::instance().emit(Topic::FactoryResetRequested);
      }
    } else if (!down && st.pressed) {
      st.pressed = false;
      st.factoryResetArmed = false;
      uint32_t heldMs = now - st.pressStartMs;
      if (!st.longFired) {
        if (st.awaitingSecondClick && (now - st.lastReleaseMs) <= (uint32_t)cfg.doublePressGapMs) {
          st.awaitingSecondClick = false;
          emitPress(which, "DOUBLE");
        } else {
          st.awaitingSecondClick = true;
          st.lastReleaseMs = now;
          st.pendingShortHeldMs = heldMs;
        }
      }
    }

    // Resolve a lone short press once the double-click window has elapsed
    // without a second click.
    if (st.awaitingSecondClick && (now - st.lastReleaseMs) > (uint32_t)cfg.doublePressGapMs) {
      st.awaitingSecondClick = false;
      emitPress(which, "SHORT");
    }
  }

  void emitPress(int which, const char* kind) {
    auto& cfg = ConfigManager::instance().get();
    const ButtonConfig& bc = (which == 1) ? cfg.button1 : cfg.button2;
    String action = (String(kind) == "SHORT") ? bc.shortAction
                    : (String(kind) == "LONG") ? bc.longAction
                    : bc.doubleAction;
    EventPayload p(kind);
    p.str2 = action;
    EventBus::instance().emit(which == 1 ? Topic::Button1Event : Topic::Button2Event, p);
  }
};
