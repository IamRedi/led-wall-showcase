#include "render/body_topology.h"

namespace ledwall {

BodyPoint BodyTopology::ascent(uint16_t phase, uint16_t tail_len, int16_t dest_x,
                               int16_t dest_y, uint16_t tail_share) const {
  BodyPoint p;
  const uint16_t n = tail_len == 0 ? 1 : tail_len;
  if (tail_share == 0) tail_share = 1;
  if (tail_share >= 65535) tail_share = 65534;
  if (phase < tail_share) {
    p.on_tail = true;
    p.tail_index = static_cast<uint16_t>((static_cast<uint32_t>(n - 1) *
                                          (tail_share - phase)) / tail_share);
    return p;
  }
  p.on_tail = false;
  const uint32_t q = (static_cast<uint32_t>(phase - tail_share) * 65535u) /
                     (65535u - tail_share);
  const int16_t x0 = static_cast<int16_t>(anchor_.left);
  const int16_t y0 = static_cast<int16_t>(anchor_.row);
  p.x = static_cast<int16_t>(x0 + ((dest_x - x0) * static_cast<int32_t>(q)) / 65535);
  p.y = static_cast<int16_t>(y0 + ((dest_y - y0) * static_cast<int32_t>(q)) / 65535);
  return p;
}

void BodyTopology::plot(FrameBuffer& matrix, TailFramebuffer& tail,
                        const BodyPoint& p, Rgb color) const {
  if (p.on_tail) tail.set(p.tail_index, color);
  else matrix.set(p.x, p.y, color);
}

void BodyTopology::light_anchor(FrameBuffer& matrix, Rgb color) const {
  matrix.set(anchor_.left, anchor_.row, color);
  matrix.set(anchor_.right, anchor_.row, color);
}

}  // namespace ledwall
