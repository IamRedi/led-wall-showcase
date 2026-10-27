#pragma once

#include <cstdint>

#include "api/command.h"  // kSpectrumBands

namespace ledwall {

// LED-L009 generic external visualizer input (reconstruction brief §17/§18).
// An authorized source feeds normalized 0..255 band levels (one band per
// matrix column); the wall renders them without caring where they came from
// (phone, PC, HUGO, HA, a future ESP32-S3 microphone bridge). The future
// microphone itself is EXPLICITLY out of scope — only this semantic input
// contract and its renderer exist.
//
// Boundedness and fail-safety:
// - exactly kSpectrumBands values, each clamped to 0..255;
// - rate-limited to kMinFeedIntervalMs between accepted feeds;
// - a feed older than kTimeoutMs expires: bands decay to zero and the
//   visualizer reports inactive (no stale frozen visuals);
// - per-band display smoothing with deterministic decay (no raw flicker).
class SpectrumInput {
 public:
  static constexpr uint32_t kMinFeedIntervalMs = 25;   // max 40 feeds/s
  static constexpr uint32_t kTimeoutMs = 1500;         // silence → inactive
  static constexpr uint16_t kDecayPerSecond = 2000;    // display decay after expiry

  // Feeds `count` band values. Returns false (mutating nothing) if the feed
  // violates the rate limit or count != kSpectrumBands.
  bool feed(const uint8_t* bands, uint8_t count, uint32_t now_ms);

  // Time flow: applies expiry decay (idempotent, monotonic `now_ms`).
  void tick(uint32_t now_ms);

  // True while fresh data exists (within the timeout window).
  bool active(uint32_t now_ms) const;

  // Display value for band x (0..255): smoothed peak decay.
  uint8_t band(uint8_t x, uint32_t now_ms) const;

  void clear();

 private:
  uint8_t bands_[kSpectrumBands]{};
  uint32_t last_feed_ms_{0};
  bool has_data_{false};
};

}  // namespace ledwall
