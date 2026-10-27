#pragma once

#include <cstdint>

namespace ledwall {

// LED-L009 device-side countdown/timer state machine. First-class device
// state (reconstruction brief §6): the phone/station/controller is only a
// controller — a countdown keeps running with every client disconnected.
//
// Timebase contract (R-B02): every method takes the SAME monotonic device
// clock (millis()) as the rest of the runtime. All deadline arithmetic is
// wrap-safe across the uint32 rollover (R-B01 pattern: signed-difference
// predicate, durations bounded by kMaxDurationMs).
//
// States: IDLE (armed with duration, not counting) → RUNNING → (PAUSED) →
// FINISHED (stable end state at zero; stays until reset/stop/restart).
// Pause freezes the exact remaining milliseconds (brief example: pause at
// 7.354 shows 7.354; resume continues from 7.354, never from the duration).
class CountdownTimer {
 public:
  enum class State : uint8_t { Idle = 0, Running, Paused, Finished };
  // 99:59 is the largest "MM:SS" value that stays within the 30 px canvas
  // (5 glyphs); longer countdowns are out of product scope.
  static constexpr uint32_t kMaxDurationMs = 5999000u;  // 99 min 59 s

  void arm(uint32_t duration_ms);  // set duration → Idle (allowed in any state; resets)
  bool start(uint32_t now_ms);     // Idle→Running, Paused→resume, Finished→restart from duration
  bool pause(uint32_t now_ms);     // Running→Paused (exact remaining frozen)
  bool resume(uint32_t now_ms);    // Paused→Running (continues from frozen remaining)
  void reset(uint32_t now_ms);     // any→Idle with the stored duration
  void tick(uint32_t now_ms);      // Running: deadline reached → Finished

  State state() const { return state_; }
  uint32_t duration_ms() const { return duration_ms_; }
  // Exact remaining milliseconds (brief §7: exposed precisely even when the
  // LED display shortens the representation).
  uint32_t remaining_ms(uint32_t now_ms) const;
  bool deadline_passed(uint32_t now_ms) const;  // pure query (event emission)

  static const char* state_name(State state);

 private:
  State state_{State::Idle};
  uint32_t duration_ms_{0};
  uint32_t deadline_ms_{0};      // valid while Running (wrap-safe unsigned math)
  uint32_t paused_remaining_{0}; // valid while Paused
};

// Display formatting for the 30×10 panel (reconstruction brief §7):
//   >= 60 s → "MM:SS"      (e.g. 01:40)
//   >= 10 s → "SS"         (e.g. 42)
//   <  10 s → "S.mmm"      (e.g. 7.354)
// All forms are ≤ 5 glyphs (≤ 30 px) and therefore render statically.
// `out` must hold at least 9 bytes.
void countdown_format(uint32_t remaining_ms, char* out);

}  // namespace ledwall
