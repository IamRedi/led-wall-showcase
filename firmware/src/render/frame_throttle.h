#pragma once

#include <cstdint>

namespace ledwall {

// BYTE-FAITHFUL COPY/ADAPT of legacy `eq_render.{h,cpp}`: dirty-flag + 50 ms
// floor (~20 FPS max) so frame transmission can never starve network/command
// handling (frozen spec §12). The millis()-difference pattern is wrap-safe.
class FrameThrottle {
 public:
  static constexpr uint32_t kMinFrameIntervalMs = 50;  // heritage kLedMaxFpsMs

  bool dirty() const { return dirty_; }
  void mark_dirty() { dirty_ = true; }

  bool take_frame(uint32_t now_ms) const {
    if (!dirty_) {
      return false;
    }
    if (now_ms - last_show_ms_ < kMinFrameIntervalMs) {
      return false;
    }
    return true;
  }

  void did_show(uint32_t now_ms) {
    last_show_ms_ = now_ms;
    dirty_ = false;
  }

 private:
  bool dirty_{true};          // heritage: g_dirty = true (first frame renders)
  uint32_t last_show_ms_{0};  // heritage: g_lastShowMs = 0
};

}  // namespace ledwall
