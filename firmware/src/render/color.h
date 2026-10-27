#pragma once

#include <cstdint>

#include "render/pixel.h"

namespace ledwall {

// Deterministic color helpers for the render layer (COPY/ADAPT of the heritage
// palette intent). Heritage renderers expressed colors as FastLED CHSV/sin8,
// which only exist on the device; these equivalents are integer-only so host
// golden frames and device output are identical by construction. Heritage
// hue/saturation/value arguments are preserved; the HSV→RGB curve is a
// standard 6-sector conversion in place of FastLED's device-only rainbow
// curve (documented adaptation, internal per frozen spec §3).

// Standard 6-sector HSV→RGB. hue 0..255 around the color wheel, sat/val
// 0..255. Deterministic integer math throughout.
inline Rgb hsv2rgb(uint8_t hue, uint8_t sat, uint8_t val) {
  if (val == 0) {
    return kRgbBlack;
  }
  if (sat == 0) {
    return Rgb{val, val, val};
  }
  const uint16_t sector = hue / 43;    // 0..5
  const uint16_t offset = hue % 43;    // position within the sector
  const uint16_t ramp = (offset * 255) / 42;
  const uint16_t lo = (static_cast<uint16_t>(val) * (255u - sat)) / 255u;
  const uint16_t amp = static_cast<uint16_t>(val) - lo;
  uint16_t r = lo;
  uint16_t g = lo;
  uint16_t b = lo;
  switch (sector) {
    case 0:  // red → yellow
      r = val;
      g = lo + (amp * ramp) / 255;
      break;
    case 1:  // yellow → green
      r = lo + (amp * (255 - ramp)) / 255;
      g = val;
      break;
    case 2:  // green → cyan
      g = val;
      b = lo + (amp * ramp) / 255;
      break;
    case 3:  // cyan → blue
      g = lo + (amp * (255 - ramp)) / 255;
      b = val;
      break;
    case 4:  // blue → magenta
      r = lo + (amp * ramp) / 255;
      b = val;
      break;
    default:  // magenta → red
      r = val;
      b = lo + (amp * (255 - ramp)) / 255;
      break;
  }
  return Rgb{static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b)};
}

// Deterministic 8-bit sine over a 256-step period (sin_u8(v) ≈ 127.5 +
// 127.5·sin(2πv/256), 0..255) — the render layer's stand-in for FastLED sin8 /
// beatsin8 patterns. Full-period table (256 bytes, constant data) keeps the
// math exact and identical on host and device.
inline uint8_t sin_u8(uint8_t v) {
  static constexpr uint8_t kSinTable[256] = {
      128, 131, 134, 137, 140, 143, 146, 149, 152, 155, 158, 162, 165, 167, 170, 173,
      176, 179, 182, 185, 188, 190, 193, 196, 198, 201, 203, 206, 208, 211, 213, 215,
      218, 220, 222, 224, 226, 228, 230, 232, 234, 235, 237, 238, 240, 241, 243, 244,
      245, 246, 248, 249, 250, 250, 251, 252, 253, 253, 254, 254, 254, 255, 255, 255,
      255, 255, 255, 255, 254, 254, 254, 253, 253, 252, 251, 250, 250, 249, 248, 246,
      245, 244, 243, 241, 240, 238, 237, 235, 234, 232, 230, 228, 226, 224, 222, 220,
      218, 215, 213, 211, 208, 206, 203, 201, 198, 196, 193, 190, 188, 185, 182, 179,
      176, 173, 170, 167, 165, 162, 158, 155, 152, 149, 146, 143, 140, 137, 134, 131,
      128, 124, 121, 118, 115, 112, 109, 106, 103, 100, 97, 93,  90,  88,  85,  82,
      79,  76,  73,  70,  67,  65,  62,  59,  57,  54,  52,  49,  47,  44,  42,  40,
      37,  35,  33,  31,  29,  27,  25,  23,  21,  20,  18,  17,  15,  14,  12,  11,
      10,  9,   7,   6,   5,   5,   4,   3,   2,   2,   1,   1,   1,   0,   0,   0,
      0,   0,   0,   0,   1,   1,   1,   2,   2,   3,   4,   5,   5,   6,   7,   9,
      10,  11,  12,  14,  15,  17,  18,  20,  21,  23,  25,  27,  29,  31,  33,  35,
      37,  40,  42,  44,  47,  49,  52,  54,  57,  59,  62,  65,  67,  70,  73,  76,
      79,  82,  85,  88,  90,  93,  97,  100, 103, 106, 109, 112, 115, 118, 121, 124,
  };
  return kSinTable[v];
}

// Heritage beatsin8(12, 40, 255) equivalent for the border pulse: one full
// sine cycle per 5000 ms (12 BPM), output 40..255. `t_ms` is the
// program-relative render time.
inline uint8_t border_pulse(uint32_t t_ms) {
  const uint32_t period_ms = 5000;
  const uint8_t phase = static_cast<uint8_t>(((t_ms % period_ms) * 256u) / period_ms);
  return static_cast<uint8_t>(40u + (static_cast<uint16_t>(sin_u8(phase)) * 215u) / 255u);
}

}  // namespace ledwall
