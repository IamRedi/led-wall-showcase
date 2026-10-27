#pragma once

#include <cstdint>

namespace ledwall {

// LED-L009 button signal conditioning (pure, host-tested; the GPIO provider
// feeds it raw levels and emits the resulting events). Debounced press-edge
// detection with optional auto-repeat for continuous controls (brightness,
// menu lists).
class ButtonDecoder {
 public:
  static constexpr uint32_t kDebounceMs = 30;
  static constexpr uint32_t kRepeatDelayMs = 600;
  static constexpr uint32_t kRepeatPeriodMs = 150;

  // Resets the decoder to the given initial level.
  void begin(bool pressed_now, uint32_t now_ms);

  // Feed the current raw level; returns true exactly once per debounced
  // press edge (no repeat).
  bool feed_press(bool pressed_now, uint32_t now_ms);

  // Feed the current raw level; returns true on the debounced press edge and
  // then every kRepeatPeriodMs while held (after kRepeatDelayMs).
  bool feed_press_or_repeat(bool pressed_now, uint32_t now_ms);

  bool is_down() const { return down_; }

 private:
  bool down_{false};
  bool raw_down_{false};
  uint32_t last_raw_change_ms_{0};
  uint32_t pressed_at_ms_{0};
  uint32_t next_repeat_ms_{0};
};

}  // namespace ledwall
