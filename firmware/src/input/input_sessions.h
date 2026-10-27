#pragma once

#include <cstdint>

#include "input/input_events.h"

namespace ledwall {

// Two bounded local controller leases. Sequence and press/release edges are
// device-checked; a held button or replayed packet emits no extra event.
// Tokens coordinate local clients, not user authentication.
class InputSessions {
 public:
  enum class Result : uint8_t { Accepted, Busy, NotOwned, StaleSequence, Duplicate, QueueFull };
  static constexpr uint32_t kIdleTimeoutMs = 15000;
  Result claim(uint8_t player, uint32_t token, uint32_t now_ms);
  Result release(uint8_t player, uint32_t token);
  Result event(uint8_t player, uint32_t token, uint32_t sequence,
               InputEvent input, bool pressed, uint32_t now_ms,
               InputEventQueue* queue);
  Result heartbeat(uint8_t player, uint32_t token, uint32_t sequence, uint32_t now_ms);
  void expire(uint32_t now_ms);
  bool claimed(uint8_t player) const;
  // Bit 0=P1, bit 1=P2; consumed by runtime after HTTP polling.
  uint8_t take_lost_mask() { const uint8_t mask = lost_mask_; lost_mask_ = 0; return mask; }
 private:
  struct Slot { uint32_t token{0}, sequence{0}, last_ms{0}; uint16_t pressed{0}; };
  Slot slots_[2]{};
  uint8_t lost_mask_{0};
};

}  // namespace ledwall
