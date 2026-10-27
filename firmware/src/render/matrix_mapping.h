#pragma once

#include <cstdint>

#include "device_constants.h"

namespace ledwall {

constexpr uint8_t kMatrixWidth = LED_WALL_MATRIX_W;
constexpr uint8_t kMatrixHeight = LED_WALL_MATRIX_H;
constexpr uint16_t kLedCount = LED_WALL_LED_COUNT;
static_assert(kLedCount == kMatrixWidth * kMatrixHeight, "matrix math must match LED count");

// Heritage sentinel: out-of-bounds pixels return kLedCount (never a crash).
constexpr uint16_t kInvalidLedIndex = kLedCount;

struct OrientationFlags {
  bool flip_odd_rows;
  bool flip_y;
};

// Heritage defaults: EQ_FLIP_ODD_ROWS 1, EQ_FLIP_Y 0 (frozen spec §2).
constexpr OrientationFlags kHeritageOrientation{true, false};

// BYTE-FAITHFUL COPY/ADAPT of legacy `eq_screen.h::screenToIndex()`:
// serpentine layout, x=0 left, y=0 top, row 0 runs left→right, odd rows
// reversed, runtime flip flags applied before the row math. Out-of-bounds
// coordinates map to kInvalidLedIndex. Kept C++11-compatible to mirror the
// heritage toolchain.
inline uint16_t heritage_screen_to_index(uint8_t x, uint8_t y, bool flip_odd_rows, bool flip_y) {
  if (x >= kMatrixWidth || y >= kMatrixHeight) {
    return kInvalidLedIndex;
  }
  if (flip_y) {
    y = static_cast<uint8_t>(kMatrixHeight - 1 - y);
  }
  const uint16_t base = static_cast<uint16_t>(y) * kMatrixWidth;
  if (flip_odd_rows && (y & 1)) {
    return base + static_cast<uint16_t>(kMatrixWidth - 1 - x);
  }
  return base + x;
}

inline uint16_t screen_to_index(uint8_t x, uint8_t y, const OrientationFlags& flags) {
  return heritage_screen_to_index(x, y, flags.flip_odd_rows, flags.flip_y);
}

}  // namespace ledwall
