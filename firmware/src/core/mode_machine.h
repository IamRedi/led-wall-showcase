#pragma once

#include <cstddef>
#include <cstdint>

#include "render/display_topology.h"

namespace ledwall {

// Frozen spec §4 baseMode values. Names are the frozen lower-case identifiers.
// LED-L009 (additive extension, ADR-LED-L009): Menu / Timer / Game /
// Notification / Standby are new semantic modes; the seven §4 kinds and their
// names are unchanged.
enum class ModeKind : uint8_t {
  Idle = 0,
  Text,
  Scene,
  Animation,
  Expression,
  Spectrum,
  Paint,
  Menu,
  Timer,
  Game,
  Notification,
  Standby,
};
constexpr uint8_t kModeKindCount = 12;

const char* mode_name(ModeKind kind);
bool mode_from_name(const char* name, ModeKind* out);

constexpr size_t kMaxProgramNameLen = 48;  // registry keys + NUL (truncation-safe)

// Transient timing constraint: deadlines use wrap-safe unsigned arithmetic on
// the caller's monotonic uint32 device clock (millis()). Correctness holds for
// durations up to INT32_MAX ms (~24.8 days); larger durations are rejected
// (mutating nothing) instead of silently breaking expiry.
constexpr uint32_t kMaxTransientDurationMs = 0x7FFFFFFFu;

// Public view of the mode/state model (frozen spec §4, machine-owned subset).
// LED-L007 (additive): started_ms fields record when the current programs
// began, giving the render layer program-relative wrap-safe time (the
// heritage frameMs model). They do not affect any §4/§11 semantics.
// LED-L010 (additive): zone_target fields carry the display-zone selection
// (matrix/tail/both) commanded with scene/animation programs; every other
// kind reports Matrix.
struct ModeSnapshot {
  ModeKind base_mode{ModeKind::Idle};
  ModeKind current_mode{ModeKind::Idle};
  char base_name[kMaxProgramNameLen]{};
  char current_name[kMaxProgramNameLen]{};
  uint32_t base_started_ms{0};
  uint32_t current_started_ms{0};
  ZoneTarget base_zone_target{ZoneTarget::Matrix};
  ZoneTarget current_zone_target{ZoneTarget::Matrix};
  bool transient_active{false};
  ModeKind transient_kind{ModeKind::Idle};
  char transient_name[kMaxProgramNameLen]{};
  uint32_t transient_remaining_ms{0};
  ZoneTarget transient_zone_target{ZoneTarget::Matrix};
};

// Frozen spec §4/§11: exactly one persistent baseMode, at most one transient
// override, replace-not-queue, no stack, no queue. Malformed/unknown input
// mutates nothing. `clear()` implements the CLEAR-1 core semantics (cancel
// transient, baseMode = idle); framebuffer blanking is the renderer's job.
//
// Timebase contract (R-B02): every method takes the SAME monotonic device
// clock (millis()); there is no boot-offset concept inside the machine. All
// expiry/remaining math is wrap-safe across the uint32 rollover (R-B01).
// `snapshot(now)` is a pure function of `now`: once `now` passes the deadline
// the transient reports as expired even before the next `tick()` observes it.
class ModeMachine {
 public:
  void boot(ModeKind base_mode, uint32_t now_ms);
  bool apply_persistent(ModeKind kind, const char* name);
  // LED-L007 overload: stamps the base program start (program-relative render
  // time). Semantically identical to apply_persistent otherwise.
  bool apply_persistent(ModeKind kind, const char* name, uint32_t now_ms);
  // LED-L010 overload: also stamps the commanded zone target (matrix/tail/
  // both). Additive: existing callers keep the older forms (target = Matrix).
  bool apply_persistent(ModeKind kind, const char* name, uint32_t now_ms,
                        ZoneTarget zone_target);
  bool apply_transient(ModeKind kind, const char* name, uint32_t duration_ms, uint32_t now_ms);
  // LED-L010 overload: transient with a commanded zone target.
  bool apply_transient(ModeKind kind, const char* name, uint32_t duration_ms, uint32_t now_ms,
                       ZoneTarget zone_target);
  void clear(uint32_t now_ms);
  void tick(uint32_t now_ms);

  // Scoped operations for frozen spec §11 CLEAR-2 (display.text.clear) only:
  // text-as-base branch replaces the base without touching a concurrent
  // non-text transient; text-as-transient branch cancels the transient and
  // returns to the current base. The generic §11 rule (persistent commands
  // cancel transients) remains in apply_persistent.
  void replace_base(ModeKind kind, const char* name);
  // LED-L007 overload: stamps the replaced base program start.
  void replace_base(ModeKind kind, const char* name, uint32_t now_ms);
  void cancel_transient(uint32_t now_ms);

  // Pure time query: would an active transient be expired at `now_ms`?
  // Does not mutate; used by the runtime to emit expiry events.
  bool transient_deadline_passed(uint32_t now_ms) const;

  bool dirty() const { return dirty_; }
  void clear_dirty() { dirty_ = false; }

  ModeSnapshot snapshot(uint32_t now_ms) const;

 private:
  static void copy_name_(char* dst, const char* src);
  bool transient_expired_(uint32_t now_ms) const;

  ModeKind base_{ModeKind::Idle};
  char base_name_[kMaxProgramNameLen]{};
  uint32_t base_started_ms_{0};
  ZoneTarget base_zone_{ZoneTarget::Matrix};
  bool transient_active_{false};
  ModeKind transient_kind_{ModeKind::Idle};
  char transient_name_[kMaxProgramNameLen]{};
  uint32_t transient_started_ms_{0};
  uint32_t transient_deadline_ms_{0};
  ZoneTarget transient_zone_{ZoneTarget::Matrix};
  bool dirty_{false};
};

// Repair cycle 1 (M-L010-6): the zone target the serialized state reports for
// `animationTarget` — the CURRENT slot's target while it is a scene/animation
// program, else the BASE slot's target while the base is scene/animation (the
// contract's "any slot" rule), else nullptr (no scene/animation applies; the
// codec serializes null). Pure function of the snapshot; host-tested for
// every base/target × transient-kind × expiry combination.
const char* reported_zone_target(const ModeSnapshot& snap);

}  // namespace ledwall
