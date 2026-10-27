#include "visualizer/spectrum_input.h"

#include <cstring>

namespace ledwall {

bool SpectrumInput::feed(const uint8_t* bands, uint8_t count, uint32_t now_ms) {
  if (bands == nullptr || count != kSpectrumBands) {
    return false;  // malformed input mutates nothing
  }
  if (has_data_ && now_ms - last_feed_ms_ < kMinFeedIntervalMs) {
    return false;  // rate limit (bounded refresh)
  }
  for (uint8_t i = 0; i < kSpectrumBands; ++i) {
    // Values arrive as uint8_t — already bounded 0..255 by construction.
    bands_[i] = bands[i];
  }
  last_feed_ms_ = now_ms;
  has_data_ = true;
  return true;
}

void SpectrumInput::tick(uint32_t now_ms) {
  if (!has_data_) {
    return;
  }
  if (now_ms - last_feed_ms_ >= kTimeoutMs) {
    // Expired: deterministic decay to silence (fail-safe, no stale visuals).
    // Decay rate: kDecayPerSecond per second, scaled by elapsed ms.
    const uint32_t elapsed = now_ms - last_feed_ms_ - kTimeoutMs;
    if (elapsed == 0) {
      return;
    }
    const uint32_t drop = elapsed * kDecayPerSecond / 1000u;
    bool any = false;
    for (uint8_t i = 0; i < kSpectrumBands; ++i) {
      bands_[i] = (bands_[i] > drop) ? static_cast<uint8_t>(bands_[i] - drop) : 0;
      if (bands_[i] > 0) {
        any = true;
      }
    }
    if (!any) {
      has_data_ = false;
    }
  }
}

bool SpectrumInput::active(uint32_t now_ms) const {
  return has_data_ && now_ms - last_feed_ms_ < kTimeoutMs;
}

uint8_t SpectrumInput::band(uint8_t x, uint32_t now_ms) const {
  if (x >= kSpectrumBands) {
    return 0;  // bounds-safe
  }
  (void)now_ms;
  return bands_[x];
}

void SpectrumInput::clear() {
  std::memset(bands_, 0, sizeof(bands_));
  has_data_ = false;
}

}  // namespace ledwall
