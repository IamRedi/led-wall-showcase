#include "core/mode_machine.h"
#include "test_framework.h"

// Frozen spec §11 arbitration/transient semantics: at most one transient,
// replace-not-queue, expiry returns to baseMode, persistent commands cancel
// the transient, duration must be positive.

void test_transient_basic_lifecycle() {
  ledwall::ModeMachine m;
  m.boot(ledwall::ModeKind::Animation, 1000);  // baseMode: animation "wave"
  m.apply_persistent(ledwall::ModeKind::Animation, "wave");
  m.clear_dirty();

  CHECK(m.apply_transient(ledwall::ModeKind::Expression, "happy", 5000, 1000));
  ledwall::ModeSnapshot s = m.snapshot(1000);
  CHECK_EQ_INT(static_cast<int>(s.current_mode), static_cast<int>(ledwall::ModeKind::Expression));
  CHECK_EQ_INT(static_cast<int>(s.base_mode), static_cast<int>(ledwall::ModeKind::Animation));
  CHECK(s.transient_active);
  CHECK_EQ_INT(s.transient_remaining_ms, 5000);
  CHECK(m.dirty());

  s = m.snapshot(3500);  // mid-transient: remaining counts down, base unchanged
  CHECK_EQ_INT(s.transient_remaining_ms, 2500);
  CHECK_EQ_INT(static_cast<int>(s.base_mode), static_cast<int>(ledwall::ModeKind::Animation));

  m.clear_dirty();
  m.tick(3500);  // before deadline: no expiry, no visual change
  s = m.snapshot(3500);
  CHECK(s.transient_active);
  CHECK(!m.dirty());

  m.tick(6000);  // expiry: rendering returns to baseMode
  s = m.snapshot(6000);
  CHECK(!s.transient_active);
  CHECK_EQ_INT(static_cast<int>(s.current_mode), static_cast<int>(ledwall::ModeKind::Animation));
  CHECK_STR_EQ(s.current_name, "wave");
  CHECK(m.dirty());
}

void test_transient_expiry_boundary() {
  ledwall::ModeMachine m;
  m.boot(ledwall::ModeKind::Idle, 1000);
  CHECK(m.apply_transient(ledwall::ModeKind::Text, "", 5000, 1000));  // deadline = 6000

  m.tick(5999);
  CHECK(m.snapshot(5999).transient_active);
  CHECK_EQ_INT(m.snapshot(5999).transient_remaining_ms, 1);

  m.tick(6000);  // expiry happens exactly at the deadline (>=)
  CHECK(!m.snapshot(6000).transient_active);
  CHECK_EQ_INT(m.snapshot(6000).transient_remaining_ms, 0);
}

void test_transient_replace_no_queue() {
  ledwall::ModeMachine m;
  m.boot(ledwall::ModeKind::Idle, 1000);

  CHECK(m.apply_transient(ledwall::ModeKind::Scene, "first", 10000, 1000));
  CHECK(m.apply_transient(ledwall::ModeKind::Expression, "alert", 1000, 2000));  // replaces

  ledwall::ModeSnapshot s = m.snapshot(2000);
  CHECK(s.transient_active);
  CHECK_EQ_INT(static_cast<int>(s.transient_kind), static_cast<int>(ledwall::ModeKind::Expression));
  CHECK_STR_EQ(s.transient_name, "alert");
  CHECK_EQ_INT(s.transient_remaining_ms, 1000);  // deadline rebased to 3000

  m.tick(3000);  // first transient must NOT resurface (no stack, no queue)
  s = m.snapshot(3000);
  CHECK(!s.transient_active);
  CHECK_EQ_INT(static_cast<int>(s.current_mode), static_cast<int>(ledwall::ModeKind::Idle));
}

void test_transient_persistent_cancels() {
  ledwall::ModeMachine m;
  m.boot(ledwall::ModeKind::Idle, 1000);
  CHECK(m.apply_transient(ledwall::ModeKind::Expression, "happy", 60000, 1000));
  CHECK(m.snapshot(1000).transient_active);

  CHECK(m.apply_persistent(ledwall::ModeKind::Scene, "corners"));  // cancels transient
  ledwall::ModeSnapshot s = m.snapshot(1000);
  CHECK(!s.transient_active);
  CHECK_EQ_INT(static_cast<int>(s.base_mode), static_cast<int>(ledwall::ModeKind::Scene));
  CHECK_EQ_INT(static_cast<int>(s.current_mode), static_cast<int>(ledwall::ModeKind::Scene));
}

void test_transient_zero_duration_rejected() {
  ledwall::ModeMachine m;
  m.boot(ledwall::ModeKind::Idle, 1000);
  m.clear_dirty();

  CHECK(!m.apply_transient(ledwall::ModeKind::Scene, "corners", 0, 1000));
  ledwall::ModeSnapshot s = m.snapshot(1000);
  CHECK(!s.transient_active);
  CHECK_EQ_INT(static_cast<int>(s.current_mode), static_cast<int>(ledwall::ModeKind::Idle));
  CHECK(!m.dirty());  // rejected command mutates nothing (§11)
}
