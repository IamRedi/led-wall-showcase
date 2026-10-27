#include "input/button_decoder.h"

namespace ledwall {

void ButtonDecoder::begin(bool pressed_now, uint32_t now_ms) {
  down_ = pressed_now;
  raw_down_ = pressed_now;
  last_raw_change_ms_ = now_ms;
  pressed_at_ms_ = pressed_now ? now_ms : 0;
  next_repeat_ms_ = 0;
}

bool ButtonDecoder::feed_press(bool pressed_now, uint32_t now_ms) {
  if (pressed_now != raw_down_) {
    raw_down_ = pressed_now;
    last_raw_change_ms_ = now_ms;
  }
  if (!down_ && raw_down_ && now_ms - last_raw_change_ms_ >= kDebounceMs) {
    down_ = true;
    pressed_at_ms_ = now_ms;
    next_repeat_ms_ = now_ms + kRepeatDelayMs;
    return true;  // debounced press edge
  }
  if (down_ && !raw_down_ && now_ms - last_raw_change_ms_ >= kDebounceMs) {
    down_ = false;
    next_repeat_ms_ = 0;
  }
  return false;
}

bool ButtonDecoder::feed_press_or_repeat(bool pressed_now, uint32_t now_ms) {
  if (feed_press(pressed_now, now_ms)) {
    return true;
  }
  if (down_ && next_repeat_ms_ != 0 &&
      static_cast<int32_t>(static_cast<uint32_t>(now_ms - next_repeat_ms_)) >= 0) {
    next_repeat_ms_ = now_ms + kRepeatPeriodMs;
    return true;  // auto-repeat tick while held
  }
  return false;
}

}  // namespace ledwall
