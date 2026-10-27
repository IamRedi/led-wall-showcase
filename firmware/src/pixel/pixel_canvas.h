#pragma once

#include <cstddef>
#include <cstdint>

#include "render/display_topology.h"
#include "render/framebuffer.h"

namespace ledwall {

// Product Pixel Studio canvas (ADR-LED-L011): 300 row-major logical matrix
// cells followed by 150 logical tail cells. The physical mapping and output
// ceiling remain exclusively in the driver. A complete validated snapshot is
// applied atomically; partial, stale and over-rate frames mutate nothing.
class PixelCanvas {
 public:
  static constexpr uint16_t kMatrixCells = 300;
  static constexpr uint16_t kTailCells = 150;
  static constexpr uint16_t kCells = kMatrixCells + kTailCells;
  static constexpr size_t kHexChars = static_cast<size_t>(kCells) * 6u;
  static constexpr uint32_t kLiveFloorMs = 100;

  enum class ApplyResult : uint8_t {
    Accepted = 0, InvalidFrame, StaleSession, StaleRevision, RateLimited, TailUnavailable
  };

  uint32_t begin_session();  // invalidates the previous session, leaves current output alone
  ApplyResult apply(uint32_t session, uint32_t revision, const char* rgb_hex,
                    size_t hex_chars, bool live, uint32_t now_ms);
  void render(FrameBuffer& matrix, TailFramebuffer* tail) const;
  void render_tail(TailFramebuffer& tail) const;
  bool active() const { return active_; }
  uint32_t session() const { return session_; }
  uint32_t revision() const { return revision_; }

 private:
  Rgb cells_[kCells]{};
  uint32_t session_{0};
  uint32_t revision_{0};
  uint32_t last_live_ms_{0};
  bool active_{false};
  bool live_seen_{false};
};

}  // namespace ledwall
