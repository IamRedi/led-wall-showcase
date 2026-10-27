#pragma once

#include <cstdint>

#include "render/display_topology.h"
#include "render/framebuffer.h"

namespace ledwall {

// One logical 450-cell body. Matrix coordinates retain the accepted serpentine
// mapping; Tail cell 0 touches the lower centre and cell n-1 is the free end.
// The physical outputs remain separate. No distance/absolute latch claim is
// encoded here: segment durations are chosen by each program.
struct BodyAnchor {
  uint8_t left{14};
  uint8_t right{15};
  uint8_t row{9};
};

struct BodyPoint {
  bool on_tail{true};
  uint16_t tail_index{0};
  int16_t x{14};
  int16_t y{9};
};

class BodyTopology {
 public:
  explicit BodyTopology(BodyAnchor anchor = BodyAnchor{}) : anchor_(valid(anchor) ? anchor : BodyAnchor{}) {}
  const BodyAnchor& anchor() const { return anchor_; }
  static bool valid(BodyAnchor a) {
    return a.right == a.left + 1 && a.right < kMatrixWidth && a.row < kMatrixHeight;
  }
  // 0..65535 travels free Tail end -> attachment -> Matrix destination.
  // tail_share is the normalized time allotted to the long Tail segment.
  BodyPoint ascent(uint16_t phase, uint16_t tail_len, int16_t dest_x,
                   int16_t dest_y, uint16_t tail_share = 56000) const;
  void plot(FrameBuffer& matrix, TailFramebuffer& tail, const BodyPoint& point,
            Rgb color) const;
  void light_anchor(FrameBuffer& matrix, Rgb color) const;

 private:
  BodyAnchor anchor_;
};

}  // namespace ledwall
