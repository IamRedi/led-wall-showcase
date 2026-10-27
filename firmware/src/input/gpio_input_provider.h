#pragma once

#include "input/button_decoder.h"
#include "input/input_events.h"

#include "input/input_pins.h"

#if defined(ARDUINO)
#include <Arduino.h>
#endif

namespace ledwall {

// LED-L009 physical input provider (device-side glue; the pure decoder is
// host-tested). Reads the (future) joystick/button shield through the
// candidate pin map and pushes normalized events into the shared queue.
// The provider is INERT unless enabled via config (input.enabled) — no pins
// are touched before the Owner confirms the shield wiring (input_pins.h).
class GpioInputProvider {
 public:
  void begin(bool enabled, const input_pins::PinMap& map);
  void poll(uint32_t now_ms, InputEventQueue* out);

 private:
#if defined(ARDUINO)
  ButtonDecoder buttons_[kInputEventEnumCount];
  bool enabled_{false};
  input_pins::PinMap map_{};
  bool axes_initialized_{false};
#endif
};

}  // namespace ledwall
