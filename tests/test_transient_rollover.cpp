#include "core/mode_machine.h"
#include "test_framework.h"

// R-B01 regression tests: transient deadline/expiry must be wrap-safe across
// the uint32 millis() rollover (frozen spec §11). Expected values are derived
// from unsigned 32-bit modular arithmetic, not from the implementation.

namespace {

// 0xFFFFF000 = 4 294 963 200; +5000 wraps the uint32 clock to deadline 904.
constexpr uint32_t kNearMax = 0xFFFFF000u;
constexpr uint32_t kWrappedDeadline = 904u;  // (0xFFFFF000 + 5000) mod 2^32

ledwall::ModeMachine booted() {
  ledwall::ModeMachine m;
  m.boot(ledwall::ModeKind::Animation, 1000);
  m.apply_persistent(ledwall::ModeKind::Animation, "wave");
  m.clear_dirty();
  return m;
}

}  // namespace

void test_transient_creation_near_uint32_max() {
  ledwall::ModeMachine m = booted();
  CHECK(m.apply_transient(ledwall::ModeKind::Expression, "alert", 5000, kNearMax));
  ledwall::ModeSnapshot s = m.snapshot(kNearMax);
  CHECK(s.transient_active);
  CHECK_EQ_INT(s.transient_remaining_ms, 5000);
  CHECK_EQ_INT(static_cast<int>(s.current_mode), static_cast<int>(ledwall::ModeKind::Expression));
  CHECK(m.dirty());
  m.clear_dirty();

  // Reviewer probe scenario: 1 ms after creation near the ceiling, the
  // transient must still be active (pre-repair code expired it instantly).
  m.tick(kNearMax + 1);
  s = m.snapshot(kNearMax + 1);
  CHECK(s.transient_active);
  CHECK_EQ_INT(s.transient_remaining_ms, 4999);
  CHECK(!m.dirty());  // no expiry, no visual change
}

void test_transient_tick_before_rollover() {
  ledwall::ModeMachine m = booted();
  CHECK(m.apply_transient(ledwall::ModeKind::Expression, "alert", 5000, kNearMax));
  m.tick(kNearMax + 200);  // 200 ms elapsed, still below the rollover
  ledwall::ModeSnapshot s = m.snapshot(kNearMax + 200);
  CHECK(s.transient_active);
  CHECK_EQ_INT(s.transient_remaining_ms, 4800);
  CHECK_EQ_INT(static_cast<int>(s.current_mode), static_cast<int>(ledwall::ModeKind::Expression));
}

void test_transient_tick_after_rollover() {
  ledwall::ModeMachine m = booted();
  CHECK(m.apply_transient(ledwall::ModeKind::Expression, "alert", 5000, kNearMax));
  m.clear_dirty();  // creation-dirty observed; subsequent dirty must mean expiry

  // After the rollover but before the wrapped deadline: still active.
  m.tick(800);
  ledwall::ModeSnapshot s = m.snapshot(800);
  CHECK(s.transient_active);
  CHECK_EQ_INT(s.transient_remaining_ms, kWrappedDeadline - 800);  // 104
  CHECK_EQ_INT(static_cast<int>(s.current_mode), static_cast<int>(ledwall::ModeKind::Expression));
  CHECK(!m.dirty());
}

void test_transient_expiry_exactly_at_wrapped_deadline() {
  ledwall::ModeMachine m = booted();
  CHECK(m.apply_transient(ledwall::ModeKind::Expression, "alert", 5000, kNearMax));

  m.tick(kWrappedDeadline - 1);
  CHECK(m.snapshot(kWrappedDeadline - 1).transient_active);
  CHECK_EQ_INT(m.snapshot(kWrappedDeadline - 1).transient_remaining_ms, 1);

  m.tick(kWrappedDeadline);  // exact boundary (>= semantics, wrap-safe)
  ledwall::ModeSnapshot s = m.snapshot(kWrappedDeadline);
  CHECK(!s.transient_active);
  CHECK_EQ_INT(s.transient_remaining_ms, 0);
  CHECK_EQ_INT(static_cast<int>(s.current_mode), static_cast<int>(ledwall::ModeKind::Animation));
  CHECK_STR_EQ(s.current_name, "wave");  // expiry returned to the baseMode
  CHECK(m.dirty());
}

void test_transient_snapshot_purity_across_rollover() {
  // snapshot(now) must be a pure function of now: an expired-by-time transient
  // reports as expired even before the next tick() observes it.
  ledwall::ModeMachine m = booted();
  CHECK(m.apply_transient(ledwall::ModeKind::Expression, "alert", 512, 0xFFFFFF00u));
  // deadline = (0xFFFFFF00 + 512) mod 2^32 = 256
  ledwall::ModeSnapshot s = m.snapshot(0xFFFFFF01u);  // 1 ms after creation
  CHECK(s.transient_active);
  CHECK_EQ_INT(s.transient_remaining_ms, 511);
  s = m.snapshot(255);  // after rollover, 1 ms before the deadline
  CHECK(s.transient_active);
  CHECK_EQ_INT(s.transient_remaining_ms, 1);
  s = m.snapshot(256);  // exactly at the deadline
  CHECK(!s.transient_active);
  CHECK_EQ_INT(static_cast<int>(s.current_mode), static_cast<int>(ledwall::ModeKind::Animation));
}

void test_transient_replacement_across_rollover() {
  ledwall::ModeMachine m = booted();
  // t1: created near the ceiling, deadline wraps to 9744.
  CHECK(m.apply_transient(ledwall::ModeKind::Scene, "first", 10000, 0xFFFFFF00u));
  // t2 replaces t1 shortly before the rollover; deadline wraps to 984.
  CHECK(m.apply_transient(ledwall::ModeKind::Expression, "alert", 1000, 0xFFFFFFF0u));
  ledwall::ModeSnapshot s = m.snapshot(0xFFFFFFF0u);
  CHECK_EQ_INT(static_cast<int>(s.transient_kind), static_cast<int>(ledwall::ModeKind::Expression));
  CHECK_STR_EQ(s.transient_name, "alert");
  CHECK_EQ_INT(s.transient_remaining_ms, 1000);

  m.tick(984);  // t2 expires; t1 must NOT resurface (no stack, no queue)
  s = m.snapshot(984);
  CHECK(!s.transient_active);
  CHECK_EQ_INT(static_cast<int>(s.current_mode), static_cast<int>(ledwall::ModeKind::Animation));
  CHECK_STR_EQ(s.current_name, "wave");
}

void test_transient_invalid_durations_unchanged_semantics() {
  ledwall::ModeMachine m = booted();
  m.clear_dirty();

  CHECK(!m.apply_transient(ledwall::ModeKind::Scene, "x", 0, kNearMax));          // zero
  CHECK(!m.apply_transient(ledwall::ModeKind::Scene, "x", 0x80000000u, kNearMax)); // > INT32_MAX
  ledwall::ModeSnapshot s = m.snapshot(kNearMax);
  CHECK(!s.transient_active);
  CHECK(!m.dirty());  // rejected input mutates nothing (§11)

  // Persistent command still cancels an active wrapped transient.
  CHECK(m.apply_transient(ledwall::ModeKind::Scene, "x", 5000, kNearMax));
  CHECK(m.apply_persistent(ledwall::ModeKind::Scene, "corners"));
  s = m.snapshot(kNearMax);
  CHECK(!s.transient_active);
  CHECK_EQ_INT(static_cast<int>(s.base_mode), static_cast<int>(ledwall::ModeKind::Scene));
}
