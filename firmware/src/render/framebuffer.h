#pragma once

#include <cstdint>

#include "render/matrix_mapping.h"
#include "render/pixel.h"

namespace ledwall {

// The renderer layer's 30×10 logical framebuffer (frozen spec §4 internal
// render state). Every pixel write goes through the accepted matrix mapping
// abstraction: out-of-bounds coordinates are dropped (heritage screenSet
// clipping), so renderer bugs can never write random LED indexes. The driver
// reads the buffer only at show time.
class FrameBuffer {
 public:
  FrameBuffer() { clear(); }

  void set_orientation(const OrientationFlags& flags) { orientation_ = flags; }

  void clear() {
    for (uint16_t i = 0; i < kLedCount; ++i) {
      pixels_[i] = kRgbBlack;
    }
  }

  // Bounds-checked logical-pixel write; negative or out-of-canvas coordinates
  // are ignored (no crash, no aliasing).
  void set(int16_t x, int16_t y, Rgb color) {
    if (x < 0 || y < 0 || x >= static_cast<int16_t>(kMatrixWidth) ||
        y >= static_cast<int16_t>(kMatrixHeight)) {
      return;
    }
    const uint16_t index = screen_to_index(static_cast<uint8_t>(x), static_cast<uint8_t>(y),
                                           orientation_);
    if (index < kLedCount) {
      pixels_[index] = color;
    }
  }

  Rgb at(uint8_t x, uint8_t y) const {
    const uint16_t index = screen_to_index(x, y, orientation_);
    return (index < kLedCount) ? pixels_[index] : kRgbBlack;
  }

  // LED-L010: bounds-checked rectangle fill (clipped to the canvas; negative
  // or oversize rects are simply clipped, never aliased).
  void fill_rect(int16_t x, int16_t y, uint8_t w, uint8_t h, Rgb color) {
    for (int16_t dy = 0; dy < static_cast<int16_t>(h); ++dy) {
      for (int16_t dx = 0; dx < static_cast<int16_t>(w); ++dx) {
        set(static_cast<int16_t>(x + dx), static_cast<int16_t>(y + dy), color);
      }
    }
  }

  const Rgb* data() const { return pixels_; }
  void set_frame_id(uint32_t id) { frame_id_ = id; }
  uint32_t frame_id() const { return frame_id_; }

 private:
  Rgb pixels_[kLedCount];
  OrientationFlags orientation_{kHeritageOrientation};
  uint32_t frame_id_{0};
};

}  // namespace ledwall
