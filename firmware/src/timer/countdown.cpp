#include "timer/countdown.h"

#include <cstdio>

namespace ledwall {

namespace {
// Wrap-safe "deadline reached" predicate (R-B01 pattern, mode_machine parity):
// (now - deadline) as a signed 32-bit distance is negative while the deadline
// is ahead (including across the uint32 rollover) and >= 0 once reached.
inline bool deadline_reached(uint32_t now_ms, uint32_t deadline_ms) {
  return static_cast<int32_t>(static_cast<uint32_t>(now_ms - deadline_ms)) >= 0;
}
}  // namespace

void CountdownTimer::arm(uint32_t duration_ms) {
  if (duration_ms == 0 || duration_ms > kMaxDurationMs) {
    return;  // invalid duration mutates nothing (dispatcher validates first)
  }
  duration_ms_ = duration_ms;
  state_ = State::Idle;
}

bool CountdownTimer::start(uint32_t now_ms) {
  if (duration_ms_ == 0) {
    return false;  // never armed: nothing to count
  }
  switch (state_) {
    case State::Idle:
      deadline_ms_ = now_ms + duration_ms_;  // natural wrap; compare wrap-safe
      state_ = State::Running;
      return true;
    case State::Paused: {
      deadline_ms_ = now_ms + paused_remaining_;
      state_ = State::Running;
      return true;
    }
    case State::Finished:
      deadline_ms_ = now_ms + duration_ms_;
      state_ = State::Running;
      return true;
    case State::Running:
      break;
  }
  return false;  // already running
}

bool CountdownTimer::pause(uint32_t now_ms) {
  if (state_ != State::Running) {
    return false;
  }
  paused_remaining_ = remaining_ms(now_ms);  // exact freeze at pause instant
  state_ = State::Paused;
  return true;
}

bool CountdownTimer::resume(uint32_t now_ms) {
  if (state_ != State::Paused) {
    return false;
  }
  deadline_ms_ = now_ms + paused_remaining_;
  state_ = State::Running;
  return true;
}

void CountdownTimer::reset(uint32_t now_ms) {
  (void)now_ms;
  state_ = State::Idle;
}

void CountdownTimer::tick(uint32_t now_ms) {
  if (state_ == State::Running && deadline_reached(now_ms, deadline_ms_)) {
    state_ = State::Finished;  // stable end state; display shows 0.000
  }
}

uint32_t CountdownTimer::remaining_ms(uint32_t now_ms) const {
  switch (state_) {
    case State::Idle:
      return duration_ms_;
    case State::Running: {
      if (deadline_reached(now_ms, deadline_ms_)) {
        return 0;
      }
      return deadline_ms_ - now_ms;  // wrap-safe unsigned difference
    }
    case State::Paused:
      return paused_remaining_;
    case State::Finished:
      return 0;
  }
  return 0;
}

bool CountdownTimer::deadline_passed(uint32_t now_ms) const {
  return state_ == State::Running && deadline_reached(now_ms, deadline_ms_);
}

const char* CountdownTimer::state_name(State state) {
  switch (state) {
    case State::Idle:
      return "idle";
    case State::Running:
      return "running";
    case State::Paused:
      return "paused";
    case State::Finished:
      return "finished";
  }
  return "idle";
}

void countdown_format(uint32_t remaining_ms, char* out) {
  const uint32_t total_seconds = remaining_ms / 1000u;
  if (total_seconds >= 60u) {
    std::snprintf(out, 9, "%02u:%02u", static_cast<unsigned>(total_seconds / 60u),
                  static_cast<unsigned>(total_seconds % 60u));
  } else if (total_seconds >= 10u) {
    std::snprintf(out, 9, "%02u", static_cast<unsigned>(total_seconds));
  } else {
    std::snprintf(out, 9, "%u.%03u", static_cast<unsigned>(total_seconds),
                  static_cast<unsigned>(remaining_ms % 1000u));
  }
}

}  // namespace ledwall
