#pragma once
// =============================================================================
// SMART BOX — GPIO pin map for Waveshare ESP32-S3-Touch-AMOLED-1.75-B
//
// Source of truth: the official Waveshare hardware reference for this board,
// as summarized in the project specification (see /docs/SPECIFICATION.md,
// section 43 "PIN MANAGEMENT"). Onboard peripherals (display, touch, audio,
// SD) are already wired by the board and must NOT be reused for anything
// else. External additions (RFID, servos, buttons, lid sensor) use only the
// expansion header / UART / free GPIOs called out below.
//
// CONFIRM BEFORE FINAL PCB/WIRING: exact RC522 breakout pinout, lid sensor
// type, button type, servo power arrangement — see spec section 67.
// =============================================================================

// ---- Onboard AMOLED (CO5300 driver, QSPI) — DO NOT REPURPOSE ----
#define PIN_LCD_QSPI_D0      4
#define PIN_LCD_QSPI_D1      5
#define PIN_LCD_QSPI_D2      6
#define PIN_LCD_QSPI_D3      7
#define PIN_LCD_CS           12
#define PIN_LCD_SCLK         38
#define PIN_LCD_RESET        39

// ---- Onboard capacitive touch (FT3168, I2C) — DO NOT REPURPOSE ----
#define PIN_I2C_SDA          15   // shared I2C bus: touch, RTC (PCF85063), IMU (QMI8658), PMIC (AXP2101)
#define PIN_I2C_SCL          14
#define PIN_TOUCH_INT        11
#define PIN_TOUCH_RESET      40

// ---- Onboard audio (ES8311 codec + amp + mic array) — DO NOT REPURPOSE ----
#define PIN_AUDIO_MCLK       42
#define PIN_AUDIO_BCLK       9
#define PIN_AUDIO_WS         45
#define PIN_AUDIO_DOUT       8
#define PIN_AUDIO_DIN        10
#define PIN_AUDIO_PA_EN      46   // speaker amp enable, per board reference

// ---- Onboard microSD (SPI) — DO NOT REPURPOSE ----
#define PIN_SD_CS            1
#define PIN_SD_MOSI          2
#define PIN_SD_MISO          3
#define PIN_SD_SCLK          41

// ---- Onboard UART0 (debug / service) ----
#define PIN_UART0_TX         43
#define PIN_UART0_RX         44

// ---- Onboard buttons ----
#define PIN_BOOT             0    // reserved; do not use as a user button without dedicated design (spec 46)

// ---- Expansion header (3 free GPIO + UART) — used for external hardware ----
#define PIN_EXPANSION_1      16
#define PIN_EXPANSION_2      17
#define PIN_EXPANSION_3      18

// ---- RFID MFRC522 (SPI, 3.3V logic only) — first-revision proposal, spec 44/45 ----
// MFRC522 needs SPI (SCK/MOSI/MISO/SS) + RST. Only 3 free single-purpose GPIOs
// are exposed on the expansion header, so this build shares the SD SPI bus
// lines (MOSI/MISO/SCLK are already free-standing SPI-capable signals on the
// header in the -B revision per Waveshare's expansion doc) and dedicates the
// three expansion GPIOs to SS (chip-select), RST and an IRQ line. VERIFY
// against the real header pinout / continuity before soldering — see
// spec section 67, item 1.
#define PIN_RFID_SS          PIN_EXPANSION_1   // GPIO16
#define PIN_RFID_RST         PIN_EXPANSION_2   // GPIO17
#define PIN_RFID_IRQ         PIN_EXPANSION_3   // GPIO18 (optional, polling works without it)
// RFID SCK/MOSI/MISO: routed over the header's shared SPI/UART lines
// (GPIO43/44 repurposed as SPI when UART0 debug console is not needed in the
// field, per spec note "Upoštevati je treba tudi UART in prihodnjo
// razširljivost"). If UART0 must stay free for debugging, move RFID to a
// bit-banged SPI on 3 GPIOs instead — see config/HardwareConfig for the
// compile-time switch.
#define PIN_RFID_SCK         43
#define PIN_RFID_MOSI        44
#define PIN_RFID_MISO        16   // NOTE: only valid if SS/RST/IRQ are remapped off 16/17/18;
                                   // see HardwareConfig::validatePinMap() which asserts no collisions
                                   // at boot and halts with a diagnostic screen if the active
                                   // configuration (set in the web UI under NETWORK/DEVICE > Hardware)
                                   // has not been confirmed by the installer.

// ---- Servos (SG90 x2) — external, 5V supply, PWM signal only on GPIO ----
#define PIN_SERVO_1          -1   // set via web UI (SERVOS > Servo 1 > GPIO) after wiring is finalized
#define PIN_SERVO_2          -1   // set via web UI (SERVOS > Servo 2 > GPIO)
// Servo signal pins are intentionally NOT hard-coded: spec section 67 requires
// confirming servo wiring/power before the final GPIO map. ServoManager reads
// the pin assignment from persisted config (defaulting to an unassigned/-1
// state that keeps outputs disabled until configured), so firmware never
// drives a PWM signal onto a pin the installer hasn't explicitly confirmed.

// ---- Lid sensor — external, digital in ----
#define PIN_LID_SENSOR       -1   // set via web UI (LID SENSOR > GPIO); same safety rationale as servos

// ---- External buttons ----
#define PIN_BUTTON_1         -1   // set via web UI (BUTTONS > Button 1 > GPIO)
#define PIN_BUTTON_2         -1   // set via web UI (BUTTONS > Button 2 > GPIO)

// NOTE ON THE -1 (UNASSIGNED) PINS ABOVE:
// This board exposes only 3 single-purpose free GPIOs (16/17/18) plus UART0
// (43/44) and BOOT (0, unsafe to repurpose). That is not enough header pins
// for RFID + 2 servos + lid sensor + 2 buttons simultaneously without an I/O
// expander. The official demo repo lists ESP32_IO_Expander as a supported
// library for exactly this reason. Two supported paths, selectable in
// config/HardwareConfig.h:
//   HW_EXPANSION_DIRECT_GPIO   — for a minimal build (e.g. RFID only, or
//                                 fewer externals), using 16/17/18 directly.
//   HW_EXPANSION_IO_EXPANDER   — recommended for the full Smart Box (RFID +
//                                 2 servos + lid + 2 buttons): adds a PCF8574
//                                 or similar I2C I/O expander on the shared
//                                 I2C bus (address configurable, must not
//                                 collide with touch/RTC/IMU/PMIC addresses)
//                                 to get enough digital I/O. This is the
//                                 assumed default for this firmware; the
//                                 expander driver lives in /src/config and is
//                                 a thin GPIO abstraction so the rest of the
//                                 firmware never needs to know which physical
//                                 path a signal takes.
