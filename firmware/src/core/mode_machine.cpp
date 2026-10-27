#include "core/mode_machine.h"

#include <cstring>

namespace ledwall {

const char* mode_name(ModeKind kind) {
  switch (kind) {
    case ModeKind::Idle:
      return "idle";
    case ModeKind::Text:
      return "text";
    case ModeKind::Scene:
      return "scene";
    case ModeKind::Animation:
      return "animation";
    case ModeKind::Expression:
      return "expression";
    case ModeKind::Spectrum:
      return "spectrum";
    case ModeKind::Paint:
      return "paint";
    case ModeKind::Menu:
      return "menu";
    case ModeKind::Timer:
      return "timer";
    case ModeKind::Game:
      return "game";
    case ModeKind::Notification:
      return "notification";
    case ModeKind::Standby:
      return "standby";
  }
  return "unknown";
}

bool mode_from_name(const char* name, ModeKind* out) {
  if (name == nullptr) {
    return false;
  }
  for (uint8_t i = 0; i < kModeKindCount; ++i) {
    const ModeKind kind = static_cast<ModeKind>(i);
    if (std::strcmp(name, mode_name(kind)) == 0) {
      if (out != nullptr) {
        *out = kind;
      }
      return true;
    }
  }
  return false;
}

void ModeMachine::copy_name_(char* dst, const char* src) {
  dst[0] = '\0';
  if (src == nullptr) {
    return;
  }
  std::strncpy(dst, src, kMaxProgramNameLen - 1);
  dst[kMaxProgramNameLen - 1] = '\0';
}

void ModeMachine::boot(ModeKind base_mode, uint32_t now_ms) {
  base_ = base_mode;
  base_name_[0] = '\0';
  base_started_ms_ = now_ms;
  transient_active_ = false;
  dirty_ = true;  // safe boot: first (blank/safe) frame must render
}

bool ModeMachine::apply_persistent(ModeKind kind, const char* name) {
  if (static_cast<uint8_t>(kind) >= kModeKindCount) {
    return false;
  }
  base_ = kind;
  copy_name_(base_name_, name);
  base_started_ms_ = 0;  // legacy 2-arg form: no start stamp
  base_zone_ = ZoneTarget::Matrix;
  transient_active_ = false;  // persistent command cancels any transient (§11)
  dirty_ = true;
  return true;
}

bool ModeMachine::apply_persistent(ModeKind kind, const char* name, uint32_t now_ms) {
  if (!apply_persistent(kind, name)) {
    return false;
  }
  base_started_ms_ = now_ms;
  return true;
}

// LED-L010: zone-target form (additive; default-arg overloads above keep the
// accepted call sites on Matrix).
bool ModeMachine::apply_persistent(ModeKind kind, const char* name, uint32_t now_ms,
                                   ZoneTarget zone_target) {
  if (!apply_persistent(kind, name, now_ms)) {
    return false;
  }
  base_zone_ = zone_target;
  return true;
}

bool ModeMachine::apply_transient(ModeKind kind, const char* name, uint32_t duration_ms, uint32_t now_ms) {
  if (static_cast<uint8_t>(kind) >= kModeKindCount) {
    return false;
  }
  if (duration_ms == 0 || duration_ms > kMaxTransientDurationMs) {
    return false;  // a transient exists only with a positive, wrap-safe duration (§11)
  }
  transient_active_ = true;
  transient_kind_ = kind;
  copy_name_(transient_name_, name);
  transient_started_ms_ = now_ms;
  transient_deadline_ms_ = now_ms + duration_ms;  // natural uint32 wrap; compare wrap-safe
  transient_zone_ = ZoneTarget::Matrix;
  dirty_ = true;
  return true;
}

// LED-L010: transient with a commanded zone target.
bool ModeMachine::apply_transient(ModeKind kind, const char* name, uint32_t duration_ms,
                                  uint32_t now_ms, ZoneTarget zone_target) {
  if (!apply_transient(kind, name, duration_ms, now_ms)) {
    return false;
  }
  transient_zone_ = zone_target;
  return true;
}

void ModeMachine::clear(uint32_t now_ms) {
  base_ = ModeKind::Idle;
  base_name_[0] = '\0';
  base_started_ms_ = now_ms;
  transient_active_ = false;
  dirty_ = true;
}

// Wrap-safe expiry (R-B01): (now - deadline) interpreted as a signed 32-bit
// distance is negative while the deadline is ahead (including across the
// uint32 rollover) and >= 0 once the deadline is reached. Valid because
// durations are bounded by kMaxTransientDurationMs.
bool ModeMachine::transient_expired_(uint32_t now_ms) const {
  return static_cast<int32_t>(static_cast<uint32_t>(now_ms - transient_deadline_ms_)) >= 0;
}

void ModeMachine::replace_base(ModeKind kind, const char* name) {
  if (static_cast<uint8_t>(kind) >= kModeKindCount) {
    return;  // defensive: invalid kind mutates nothing
  }
  base_ = kind;
  copy_name_(base_name_, name);
  base_started_ms_ = 0;  // legacy 2-arg form: no start stamp
  base_zone_ = ZoneTarget::Matrix;
  dirty_ = true;
}

void ModeMachine::replace_base(ModeKind kind, const char* name, uint32_t now_ms) {
  replace_base(kind, name);
  base_started_ms_ = now_ms;
}

void ModeMachine::cancel_transient(uint32_t now_ms) {
  if (!transient_active_) {
    return;
  }
  transient_active_ = false;  // expiry-equivalent return to the current baseMode
  (void)now_ms;
  dirty_ = true;
}

bool ModeMachine::transient_deadline_passed(uint32_t now_ms) const {
  return transient_active_ && transient_expired_(now_ms);
}

void ModeMachine::tick(uint32_t now_ms) {
  if (transient_active_ && transient_expired_(now_ms)) {
    transient_active_ = false;  // expiry returns rendering to the baseMode
    dirty_ = true;
  }
}

ModeSnapshot ModeMachine::snapshot(uint32_t now_ms) const {
  ModeSnapshot s;
  s.base_mode = base_;
  std::strncpy(s.base_name, base_name_, kMaxProgramNameLen);
  s.base_started_ms = base_started_ms_;
  s.base_zone_target = base_zone_;
  const bool active_now = transient_active_ && !transient_expired_(now_ms);
  if (active_now) {
    s.current_mode = transient_kind_;
    std::strncpy(s.current_name, transient_name_, kMaxProgramNameLen);
    s.current_started_ms = transient_started_ms_;
    s.current_zone_target = transient_zone_;
    s.transient_active = true;
    s.transient_kind = transient_kind_;
    std::strncpy(s.transient_name, transient_name_, kMaxProgramNameLen);
    s.transient_remaining_ms = transient_deadline_ms_ - now_ms;  // wrap-safe unsigned
    s.transient_zone_target = transient_zone_;
  } else {
    s.current_mode = base_;
    std::strncpy(s.current_name, base_name_, kMaxProgramNameLen);
    s.current_started_ms = base_started_ms_;
    s.current_zone_target = base_zone_;
  }
  return s;
}


const char* reported_zone_target(const ModeSnapshot& snap) {
  // Repair cycle 1 (M-L010-6): report the target of the program the state is
  // actually showing — the current slot while it is scene/animation (a
  // transient scene/animation overrides the base), else the base slot while
  // IT is scene/animation (e.g. a notification over a tail animation reports
  // the animation's commanded target, never a stale default), else null.
  if (snap.current_mode == ModeKind::Scene || snap.current_mode == ModeKind::Animation) {
    const char* zone = zone_target_name(snap.current_zone_target);
    return (zone != nullptr) ? zone : kZoneTargetNames[0];
  }
  if (snap.base_mode == ModeKind::Scene || snap.base_mode == ModeKind::Animation) {
    const char* zone = zone_target_name(snap.base_zone_target);
    return (zone != nullptr) ? zone : kZoneTargetNames[0];
  }
  return nullptr;
}

}  // namespace ledwall
