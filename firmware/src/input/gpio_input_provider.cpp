#include "input/gpio_input_provider.h"

namespace ledwall {

#if defined(ARDUINO)

void GpioInputProvider::begin(bool enabled, const input_pins::PinMap& map) {
  enabled_ = enabled;
  map_ = map;
  if (!enabled_) {
    return;  // inert: no pin configuration before Owner-confirmed wiring
  }
  const int pins[kInputEventEnumCount] = {map_.up,      map_.down,  map_.left,
                                          map_.right,   map_.confirm, map_.back,
                                          map_.start_pause, map_.home};
  const uint32_t now = millis();
  for (uint8_t i = 0; i < kInputEventEnumCount; ++i) {
    if (pins[i] < 0) {
      continue;
    }
    pinMode(static_cast<uint8_t>(pins[i]), INPUT_PULLUP);
    const bool pressed = (digitalRead(static_cast<uint8_t>(pins[i])) == LOW) && map_.active_low;
    buttons_[i].begin(pressed, now);
  }
}

void GpioInputProvider::poll(uint32_t now_ms, InputEventQueue* out) {
  if (!enabled_ || out == nullptr) {
    return;
  }
  const int pins[kInputEventEnumCount] = {map_.up,      map_.down,  map_.left,
                                          map_.right,   map_.confirm, map_.back,
                                          map_.start_pause, map_.home};
  for (uint8_t i = 0; i < kInputEventEnumCount; ++i) {
    if (pins[i] < 0) {
      continue;
    }
    const bool level_low = digitalRead(static_cast<uint8_t>(pins[i])) == LOW;
    const bool pressed = map_.active_low ? level_low : !level_low;
    // Auto-repeat only for the directional pad; action buttons fire on edge.
    const bool event =
        (i <= static_cast<uint8_t>(InputEvent::Right))
            ? buttons_[i].feed_press_or_repeat(pressed, now_ms)
            : buttons_[i].feed_press(pressed, now_ms);
    if (event) {
      out->push(static_cast<InputEvent>(i));
    }
  }
  // Analog joystick axes (if wired): ADC1 pins, deadbanded around mid-rail.
  // Only direction changes push events (edge-triggered, no repeat).
  static bool axis_state[2] = {false, false};
  const int axes[2] = {map_.axis_x, map_.axis_y};
  const InputEvent axis_events[2][2] = {{InputEvent::Left, InputEvent::Right},
                                        {InputEvent::Up, InputEvent::Down}};
  for (uint8_t a = 0; a < 2; ++a) {
    if (axes[a] < 0) {
      continue;
    }
    if (!axes_initialized_) {
      pinMode(static_cast<uint8_t>(axes[a]), INPUT);
    }
    const uint32_t value = analogRead(static_cast<uint8_t>(axes[a]));
    const bool negative = value + input_pins::kAxisDeadband < input_pins::kAxisCenter;
    const bool positive = value > input_pins::kAxisCenter + input_pins::kAxisDeadband;
    const bool active = negative || positive;
    const uint8_t dir = negative ? 0u : 1u;
    if (active && !axis_state[a]) {
      out->push(axis_events[a][dir]);
    }
    axis_state[a] = active;
  }
  axes_initialized_ = true;
}

#else

// Host build: the GPIO provider is device-only glue; the pure decoders and
// queue are exercised directly by the host suites.
void GpioInputProvider::begin(bool /*enabled*/, const input_pins::PinMap& /*map*/) {}
void GpioInputProvider::poll(uint32_t /*now_ms*/, InputEventQueue* /*out*/) {}

#endif

}  // namespace ledwall
