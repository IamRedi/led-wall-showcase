#pragma once

#include <cstdint>

#include "device_constants.h"
#include "render/pixel.h"

namespace ledwall {

// LED-L010 display topology (ADR-LED-L010 D1; expansion brief §3/§5). The
// product owns TWO logical surfaces: the 30×10 matrix (accepted FrameBuffer)
// and the 1D tail rope. Every animation/game render path declares or is
// commanded to a zone target; product logic never touches FastLED arrays,
// GPIO, or physical ordering — the driver layer owns all of that.

enum class ZoneTarget : uint8_t {
  Matrix = 0,
  Tail,
  Both,
};
constexpr uint8_t kZoneTargetCount = 3;
constexpr const char* const kZoneTargetNames[kZoneTargetCount] = {
    "matrix", "tail", "both",
};

const char* zone_target_name(ZoneTarget target);
bool zone_target_from_name(const char* name, ZoneTarget* out);

// Per-animation supported-target mask (registry metadata; D3). Bit flags so
// "both" can be supported independently of "tail"/"matrix".
namespace zone_flags {
constexpr uint8_t kMatrix = 0x1;
constexpr uint8_t kTail = 0x2;
constexpr uint8_t kBoth = 0x4;
}  // namespace zone_flags

// True iff requesting `requested` for a program whose supported mask is
// `supported_mask` is valid (e.g. requested=Both requires the kBoth flag;
// requested=Tail requires kTail; requested=Matrix requires kMatrix).
bool zone_target_supported(uint8_t supported_mask, ZoneTarget requested);

// Tail zone configuration (dotted config vocabulary: tail.enabled,
// tail.length, tail.reverse). The physical LED count is a hardware fact —
// `length` defaults to the documented PLACEHOLDER and must be set to the
// Owner-counted value; software never infers LEDs per metre.
struct TailZoneConfig {
  bool enabled{false};  // ships disabled until the Owner wires the rope
  uint16_t length{LED_WALL_TAIL_LENGTH_DEFAULT};
  bool reverse{false};  // physical direction flip, applied in the driver
};

// The 1D logical tail surface (LED-L010). Bounds-checked like FrameBuffer:
// out-of-range cell writes are dropped, so renderer/game bugs can never touch
// random indexes. `set_orientation` is the driver's reverse flag mirrored for
// logical-coordinate queries; the DRIVER applies the physical flip.
class TailFramebuffer {
 public:
  static constexpr uint16_t kMaxLength = LED_WALL_TAIL_MAX_LENGTH;

  TailFramebuffer() { clear(); }

  void configure(uint16_t length) {
    length_ = (length >= 1 && length <= kMaxLength) ? length : 1;
  }

  void clear() {
    for (uint16_t i = 0; i < kMaxLength; ++i) {
      cells_[i] = kRgbBlack;
    }
  }

  void fill(Rgb color) {
    for (uint16_t i = 0; i < kMaxLength; ++i) {
      cells_[i] = color;
    }
  }

  // Bounds-checked logical-cell write (0..length()-1); out-of-range ignored.
  void set(uint16_t cell, Rgb color) {
    if (cell >= length_) {
      return;
    }
    cells_[cell] = color;
  }

  Rgb at(uint16_t cell) const {
    return (cell < length_) ? cells_[cell] : kRgbBlack;
  }

  uint16_t length() const { return length_; }

  const Rgb* data() const { return cells_; }
  void set_frame_id(uint32_t id) { frame_id_ = id; }
  uint32_t frame_id() const { return frame_id_; }

 private:
  Rgb cells_[kMaxLength]{};
  uint16_t length_{LED_WALL_TAIL_LENGTH_DEFAULT};
  uint32_t frame_id_{0};
};

}  // namespace ledwall
