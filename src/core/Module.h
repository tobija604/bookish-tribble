#pragma once
// Common interface every subsystem implements so main.cpp's loop() is a flat,
// non-blocking dispatch list (spec 59: "Vsaka komponenta naj ima svoj state
// machine/task"). Each module's loop() must return quickly — no delay(),
// no blocking network/SPI calls without a timeout.
class IModule {
 public:
  virtual ~IModule() = default;
  virtual void begin() = 0;
  virtual void loop() = 0;
  virtual const char* name() const = 0;
};
