# Hardware notes — read before wiring anything

## Confirmed from the official Waveshare wiki/docs for this board

- AMOLED driver IC: **CO5300** (QSPI interface, 466×466, round)
- Touch controller: **FT3168** (I2C; some wiki revisions mention CST9217 —
  confirm which your specific unit shipped with before trusting the
  register map in `src/touch/Ft3168Touch.h`)
- Audio codec: **ES8311** (I2S + I2C control)
- RTC: **PCF85063**
- IMU: **QMI8658**
- Power management: **AXP2101**
- Official demo repo: `github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.75`
  (Arduino_GFX / LVGL v8.4.0 / SensorLib / XPowersLib / ESP32_IO_Expander)

## GPIO map

See `include/pins.h` — it is the single source of truth and is heavily
commented. Summary:

| Function | Pins |
|---|---|
| AMOLED QSPI | GPIO4-7 (data), GPIO12 (CS), GPIO38 (SCLK), GPIO39 (reset) |
| Touch (FT3168) | GPIO14/15 (shared I2C), GPIO11 (INT), GPIO40 (reset) |
| Audio (ES8311) | GPIO8/9/10/42/45/46 |
| microSD | GPIO1/2/3/41 |
| UART0 | GPIO43/44 |
| BOOT | GPIO0 (do not repurpose) |
| Expansion header | GPIO16/17/18 (only 3 free single-purpose GPIOs) |

**The hard constraint**: this board exposes only 3 free GPIOs plus UART0.
The full Smart Box needs GPIO for RFID (SPI: 4 lines + optional IRQ), 2×
servo PWM, a lid sensor, and 2 buttons — more than 3 pins' worth. Two paths
are wired into the firmware (`src/config/HardwareConfig.h`):

1. **DirectGpio** — use the header GPIOs directly for a reduced build
   (e.g. RFID only).
2. **IoExpander** (default) — add an I2C GPIO expander (PCF8574 or similar)
   on the shared I2C bus for the digital externals (lid sensor, buttons,
   RFID RST/IRQ), while servo PWM and RFID SPI clock/data still need real
   GPIOs. **This still needs a concrete wiring decision before a PCB is
   made** — see spec section 67, item by item. `HardwareConfig` is a thin
   abstraction so the rest of the firmware doesn't care which path is used.

## What must be verified against real hardware before trusting this code

- `src/display/DisplayManager.h`: `Arduino_ESP32QSPI` / `Arduino_CO5300`
  constructor argument order, and whether brightness control needs a
  vendor-specific command (AMOLEDs are self-emissive — there's no PWM
  backlight pin the way there is on an LCD).
- `src/touch/Ft3168Touch.h`: I2C address (0x38 assumed) and register
  layout (assumed FT5x06-family layout).
- `src/audio/Es8311Codec.h`: the actual ES8311 register init sequence is
  a stub — port it from the Waveshare demo repo.
- `src/rtc/RtcManager.h`, `src/imu/ImuManager.h`, `src/power/PowerManager.h`:
  written against the *expected* SensorLib/XPowersLib API shape; check
  method names against whatever version PlatformIO actually resolves
  (these libraries have changed their API across releases).
- RFID wiring (`include/pins.h`, "PREDLAGANA ZUNANJA POVEZAVA" section):
  the SCK/MOSI lines borrowing UART0 is a first-revision proposal, not a
  verified schematic.
- Servo GPIOs, lid sensor GPIO, and button GPIOs are all `-1` (unassigned)
  by default and must be set from the web UI (SERVOS / LID SENSOR /
  BUTTONS) once the physical wiring is finalized — the firmware refuses to
  drive an unconfigured pin.
