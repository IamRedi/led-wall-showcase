#pragma once

#include <cstdint>

namespace ledwall {

// Device-independent RGB pixel (render layer currency). The render modules are
// pure C++ so they compile and test identically on host and device; the LED
// driver converts RGB to FastLED CRGB at the output boundary only.
// Members carry no default initializers on purpose: the type stays a plain
// aggregate (brace-init works across host and ESP32 toolchain dialects).
struct Rgb {
  uint8_t r;
  uint8_t g;
  uint8_t b;
};

inline bool operator==(const Rgb& a, const Rgb& b) {
  return a.r == b.r && a.g == b.g && a.b == b.b;
}
inline bool operator!=(const Rgb& a, const Rgb& b) { return !(a == b); }

constexpr Rgb kRgbBlack{0, 0, 0};

}  // namespace ledwall
