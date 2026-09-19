#pragma once
// Thin GPIO abstraction so RfidManager / ServoManager / LidSensor / Buttons
// never need to know whether a "pin" is a direct ESP32-S3 GPIO or a line on
// an I2C I/O expander (see the long note at the bottom of include/pins.h —
// this board only exposes 3 free single-purpose GPIOs, not enough for the
// full external peripheral set). Selected once here; everything else just
// calls GpioLine::digitalRead/Write on the abstract line number it was given
// from ConfigManager.
#include <Arduino.h>
#include <Wire.h>

enum class ExpansionMode {
  DirectGpio,     // use ESP32-S3 GPIOs 16/17/18 (+ UART0 if freed) directly
  IoExpander,     // PCF8574-style I2C expander on the shared bus (recommended, spec 43/67)
};

class HardwareConfig {
 public:
  static HardwareConfig& instance() {
    static HardwareConfig hw;
    return hw;
  }

  ExpansionMode mode = ExpansionMode::IoExpander;
  uint8_t expanderI2cAddr = 0x20; // must not collide with touch/RTC/IMU/PMIC — validated in begin()

  bool begin() {
    if (mode == ExpansionMode::DirectGpio) return true;
    Wire.beginTransmission(expanderI2cAddr);
    bool ok = (Wire.endTransmission() == 0);
    if (!ok) {
      Serial.printf("[HardwareConfig] no I/O expander answering at 0x%02X — "
                     "falling back to DirectGpio (RFID/servo/lid/button pin "
                     "counts will be limited to onboard header GPIOs)\n",
                     expanderI2cAddr);
      mode = ExpansionMode::DirectGpio;
    }
    return true;
  }

  // Every module treats these as opaque "line numbers" per the mode above.
  // In DirectGpio mode a line number IS the ESP32 GPIO number. In IoExpander
  // mode, lines 0-7 map to expander pins P0-P7; ServoManager/etc still write
  // PWM through a dedicated ESP32 GPIO (servos need real PWM, not a slow I2C
  // expander), so in practice IoExpander mode is used for the *digital*
  // externals (lid sensor, buttons, RFID RST/IRQ) while RFID SPI and servo
  // PWM stay on real GPIOs. This split is documented in
  // docs/HARDWARE_NOTES.md and must be confirmed against the finished wiring
  // before production (spec section 67).
  void pinMode(int line, uint8_t mode_) {
    if (mode == ExpansionMode::DirectGpio) { ::pinMode(line, mode_); return; }
    // Expander direction is set by writing 1 (input, open-drain-ish quasi
    // bidirectional on PCF8574) — real driver TODO, see docs/HARDWARE_NOTES.md.
  }

  int digitalRead(int line) {
    if (mode == ExpansionMode::DirectGpio) return ::digitalRead(line);
    return readExpander(line);
  }

  void digitalWrite(int line, uint8_t val) {
    if (mode == ExpansionMode::DirectGpio) { ::digitalWrite(line, val); return; }
    writeExpander(line, val);
  }

 private:
  HardwareConfig() = default;
  uint8_t expanderShadow_ = 0xFF;

  int readExpander(int line) {
    Wire.requestFrom((int)expanderI2cAddr, 1);
    if (Wire.available()) expanderShadow_ = Wire.read();
    return (expanderShadow_ >> line) & 0x01;
  }

  void writeExpander(int line, uint8_t val) {
    if (val) expanderShadow_ |= (1 << line);
    else     expanderShadow_ &= ~(1 << line);
    Wire.beginTransmission(expanderI2cAddr);
    Wire.write(expanderShadow_);
    Wire.endTransmission();
  }
};
