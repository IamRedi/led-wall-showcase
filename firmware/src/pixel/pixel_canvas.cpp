#include "pixel/pixel_canvas.h"

namespace ledwall {
namespace {
int8_t nibble(char ch) {
  if (ch >= '0' && ch <= '9') return static_cast<int8_t>(ch - '0');
  if (ch >= 'a' && ch <= 'f') return static_cast<int8_t>(ch - 'a' + 10);
  if (ch >= 'A' && ch <= 'F') return static_cast<int8_t>(ch - 'A' + 10);
  return -1;
}
}  // namespace

uint32_t PixelCanvas::begin_session() {
  ++session_;
  if (session_ == 0) ++session_;
  revision_ = 0;
  live_seen_ = false;
  return session_;
}

PixelCanvas::ApplyResult PixelCanvas::apply(uint32_t session, uint32_t revision,
                                             const char* rgb_hex, size_t hex_chars,
                                             bool live, uint32_t now_ms) {
  if (session == 0 || session != session_) return ApplyResult::StaleSession;
  if (revision == 0 || revision <= revision_) return ApplyResult::StaleRevision;
  if (rgb_hex == nullptr || hex_chars != kHexChars) return ApplyResult::InvalidFrame;
  if (live && live_seen_ && now_ms - last_live_ms_ < kLiveFloorMs) {
    return ApplyResult::RateLimited;
  }

  Rgb staged[kCells];
  for (uint16_t i = 0; i < kCells; ++i) {
    uint8_t channels[3];
    for (uint8_t c = 0; c < 3; ++c) {
      const int8_t hi = nibble(rgb_hex[static_cast<size_t>(i) * 6u + c * 2u]);
      const int8_t lo = nibble(rgb_hex[static_cast<size_t>(i) * 6u + c * 2u + 1u]);
      if (hi < 0 || lo < 0) return ApplyResult::InvalidFrame;
      channels[c] = static_cast<uint8_t>((hi << 4) | lo);
    }
    staged[i] = Rgb{channels[0], channels[1], channels[2]};
  }
  for (uint16_t i = 0; i < kCells; ++i) cells_[i] = staged[i];
  revision_ = revision;
  active_ = true;
  if (live) {
    live_seen_ = true;
    last_live_ms_ = now_ms;
  }
  return ApplyResult::Accepted;
}

void PixelCanvas::render(FrameBuffer& matrix, TailFramebuffer* tail) const {
  matrix.clear();
  if (tail != nullptr) tail->clear();
  if (!active_) return;
  for (uint16_t i = 0; i < kMatrixCells; ++i) {
    matrix.set(static_cast<int16_t>(i % 30u), static_cast<int16_t>(i / 30u), cells_[i]);
  }
  if (tail != nullptr) {
    render_tail(*tail);
  }
}

void PixelCanvas::render_tail(TailFramebuffer& tail) const {
  tail.clear();
  if (!active_) return;
  const uint16_t count = tail.length() < kTailCells ? tail.length() : kTailCells;
  for (uint16_t i = 0; i < count; ++i) tail.set(i, cells_[kMatrixCells + i]);
}

}  // namespace ledwall
