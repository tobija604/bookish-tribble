# 🔐 SMART BOX — Celotna specifikacija sistema
### za Waveshare ESP32-S3-Touch-AMOLED-1.75-B

*(Sections 1 and 68 are quoted verbatim below as bookends; sections 2-67
are summarized with a pointer to their content, since the full 68-section
Slovenian original is long and is the conversation this project was
generated from. "spec section N" comments throughout the firmware and web
dashboard source code refer to this same numbering — keep the original
request text alongside this repo if you want the full line-by-line
wording.)*

## 1. GLAVNA STROJNA PLATFORMA

Glavni krmilnik: Waveshare ESP32-S3-Touch-AMOLED-1.75-B

Glavni MCU: ESP32-S3R8, dual-core Xtensa LX7, do 240 MHz, 8 MB PSRAM,
16 MB zunanjega Flash, Wi-Fi 2,4 GHz 802.11 b/g/n, Bluetooth 5 LE.

Plošča ima integriran: 1,75" okrogli AMOLED, ločljivost 466×466, capacitive
touch, RTC, IMU, mikrofone, audio codec, ojačevalnik, priključek za
zvočnik, microSD/TF, USB-C, PWR in BOOT tipki, AXP2101 power management,
8-pin expansion header.

GPS MODULA NE UPORABLJAJ. Gre za različico -B, ne -G.

## 2-67

See the full original Slovenian specification (68 numbered sections,
covering the AMOLED UI, touch menu, RFID, users/roles, physical buttons,
web-admin security code, servo locking, lid sensor, audio, WiFi
provisioning, offline operation, the full web dashboard menu, appearance,
diagnostics, event logging, statistics, backup/restore, automation,
access schedules, web/firmware security, OTA, microSD, Bluetooth, IMU
tamper detection, power management, mechanical design, GPIO pin
management, and the Android app) as supplied by the project owner. Each
numbered section is referenced directly in code comments as "spec section
N" — search the codebase for `spec section <N>` or `spec <N>` to find every
place a given requirement is implemented.

## 68. KONČNI CILJ

Končni Smart Box je samostojna pametna fizična škatla z dvojno mehansko
ključavnico, RFID dostopom, 1,75" AMOLED touchscreenom, Wi-Fi, Bluetooth,
zvokom, mikrofoni, RTC, statistikami, lokalnim web panelom, uporabniki,
karticami, varnostno kodo, nastavljivimi servoti, dvema zunanjima tipkama,
senzorjem pokrova, avtomatizacijo, lokalnimi dogodki, OTA firmware,
diagnostiko, popolnoma nastavljivim izgledom, večjezičnostjo in možnostjo
kasnejše mobilne/cloud razširitve.

**Osnovno pravilo: TUDI BREZ INTERNETA MORA SMART BOX NORMALNO DELOVATI.**
