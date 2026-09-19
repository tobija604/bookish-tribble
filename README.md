# 🔐 SMART BOX

Firmware + web dashboard + Android skeleton + web installer for a
touchscreen-and-RFID smart lockbox, built on the **Waveshare
ESP32-S3-Touch-AMOLED-1.75-B**.

This is a **wide-breadth skeleton**: every module named in the original
68-section specification (`docs/SPECIFICATION.md`) exists as real,
structured code with working core logic, wired end-to-end from touch/RFID/
button input through to the servo locks, the event log, and the web
dashboard. A handful of pieces that genuinely need real hardware in hand
to get right (exact display/touch/audio init sequences, final RFID/servo/
lid GPIO wiring) are left as clearly marked, documented TODOs rather than
guessed at — see `docs/HARDWARE_NOTES.md` for the full list before you
start soldering.

## Repository layout

```
platformio.ini        PlatformIO project (Arduino framework, ESP32-S3)
partitions.csv         16MB flash layout: dual-OTA app slots + LittleFS
include/pins.h         GPIO map — the single source of truth for wiring
src/
  core/                 EventBus (pub/sub), IModule interface
  config/               Config struct + ConfigManager (persisted JSON) + HardwareConfig (GPIO/IO-expander abstraction)
  storage/              LittleFS JSON load/save helper
  display/              LVGL + Arduino_GFX bring-up, screens/ (Home, Menu, Unlock, Lock, RFID, InfoScreen)
  touch/                FT3168 capacitive touch driver
  rfid/                 MFRC522 driver + card store
  servo/                Dual-SG90 lock controller
  lid/                  Lid sensor + auto-lock
  buttons/              Button debouncing/classification + action dispatch
  wifi/                 AP provisioning + STA + offline fallback
  security/             Web sessions, one-time security code, rate limiting
  users/                Users, roles, permissions, access schedules
  events/               Event log (RAM ring buffer + LittleFS persistence) + stats
  web/                  REST API + static dashboard server
  audio/                ES8311 + I2S tone playback
  rtc/                  PCF85063
  imu/                  QMI8658 tamper detection
  power/                AXP2101 battery/PMIC
  sd/                   optional microSD
  ota/                  HTTP OTA with SHA-256 verification + rollback
  diagnostics/          self-test suite
  automation/           IF/THEN rule engine
  locale/               SL/EN string tables (DE/IT/HR/SR fall back to EN)
  main.cpp               boot sequence + main loop wiring
data/                   Web dashboard (served from LittleFS): index.html, css/, js/
installer/              Browser-based flashing page (esp-web-tools) — needs a compiled firmware.bin, see installer/build/README.md
android/                 Minimal Android app skeleton (status/unlock/lock against the same REST API)
docs/                    SPECIFICATION.md, HARDWARE_NOTES.md, SECURITY_NOTES.md, UX_NOTES.md
```

## Building

```bash
pip install platformio   # or use the PlatformIO IDE extension for VS Code
cd smart-box
pio run                        # compile firmware
pio run -t buildfs             # build the LittleFS image from data/
pio run -t upload              # flash firmware (device connected via USB-C)
pio run -t uploadfs            # flash the filesystem image
pio device monitor             # serial console (115200 baud)
```

First boot with no saved WiFi credentials starts a `SMARTBOX-XXXX` open
access point at `192.168.4.1` — connect and open that address in a
browser for WIFI SETUP (spec section 22).

## What's real vs. what's a documented gap

**Working end-to-end today (logic, persistence, REST contract, UI):**
config persistence, users/roles/permissions, RFID card store + access
checking + schedules, event logging + stats, web session auth + one-time
security code, WiFi AP/STA provisioning + offline fallback, button press
classification + factory-reset combo, lid auto-lock, servo lock state
machine, automation rule engine, the full REST API, and the full web
dashboard (20 sections) talking to it.

**Needs real hardware to finish (see `docs/HARDWARE_NOTES.md` for
specifics):** the AMOLED/touch/audio hardware bring-up (driver class
names and pin wiring are correct per the official Waveshare reference, but
init parameters and the ES8311 register sequence need verification/
porting against the real chip), and the final RFID/servo/lid/button GPIO
assignments (this board only exposes 3 free GPIOs — `HardwareConfig.h`
documents the I/O-expander path this firmware assumes).

**Deliberately out of scope for this pass** (spec explicitly marks these
"kasneje"/"later" or optional): Bluetooth pairing/provisioning, a camera
module, cloud sync/multi-device dashboard, and voice control via the
onboard microphones (left disabled per spec section 20).

## Security

Read `docs/SECURITY_NOTES.md` before this guards anything that matters —
password hashing in particular is a stubbed TODO, clearly marked in
`src/web/WebServer.h`.
