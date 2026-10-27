#include <cstring>

#include "core/mode_machine.h"
#include "test_framework.h"

namespace {

ledwall::ModeMachine fresh_machine() {
  ledwall::ModeMachine m;
  m.boot(ledwall::ModeKind::Idle, 1000);
  m.clear_dirty();
  return m;
}

}  // namespace

void test_mode_machine_boot_and_persistent() {
  ledwall::ModeMachine m;
  m.boot(ledwall::ModeKind::Idle, 0);
  ledwall::ModeSnapshot s = m.snapshot(0);
  CHECK_EQ_INT(static_cast<int>(s.base_mode), static_cast<int>(ledwall::ModeKind::Idle));
  CHECK_EQ_INT(static_cast<int>(s.current_mode), static_cast<int>(ledwall::ModeKind::Idle));
  CHECK(!s.transient_active);
  CHECK(m.dirty());  // safe boot: first frame must render
  m.clear_dirty();
  CHECK(!m.dirty());

  // Persistent command replaces baseMode and stores the semantic name.
  CHECK(m.apply_persistent(ledwall::ModeKind::Animation, "wave"));
  s = m.snapshot(0);
  CHECK_EQ_INT(static_cast<int>(s.base_mode), static_cast<int>(ledwall::ModeKind::Animation));
  CHECK_EQ_INT(static_cast<int>(s.current_mode), static_cast<int>(ledwall::ModeKind::Animation));
  CHECK_STR_EQ(s.base_name, "wave");
  CHECK_STR_EQ(s.current_name, "wave");
  CHECK(m.dirty());
}

void test_mode_machine_clear() {
  ledwall::ModeMachine m = fresh_machine();
  CHECK(m.apply_persistent(ledwall::ModeKind::Scene, "corners"));
  m.clear_dirty();
  CHECK(m.apply_transient(ledwall::ModeKind::Expression, "happy", 1000, 100));
  CHECK(m.dirty());
  m.clear_dirty();

  m.clear(500);  // CLEAR-1 core: cancel transient, baseMode = idle
  ledwall::ModeSnapshot s = m.snapshot(500);
  CHECK_EQ_INT(static_cast<int>(s.base_mode), static_cast<int>(ledwall::ModeKind::Idle));
  CHECK_EQ_INT(static_cast<int>(s.current_mode), static_cast<int>(ledwall::ModeKind::Idle));
  CHECK(!s.transient_active);
  CHECK_STR_EQ(s.base_name, "");
  CHECK(m.dirty());  // framebuffer must blank on next throttled frame
}

void test_mode_machine_name_truncation() {
  ledwall::ModeMachine m = fresh_machine();
  char long_name[256];
  for (size_t i = 0; i < sizeof(long_name) - 1; ++i) {
    long_name[i] = static_cast<char>('a' + (i % 26));
  }
  long_name[sizeof(long_name) - 1] = '\0';

  CHECK(m.apply_persistent(ledwall::ModeKind::Scene, long_name));
  ledwall::ModeSnapshot s = m.snapshot(0);
  CHECK_EQ_INT(std::strlen(s.base_name), ledwall::kMaxProgramNameLen - 1);
  // Machine must remain fully operational after truncation.
  CHECK(m.apply_persistent(ledwall::ModeKind::Idle, nullptr));
  s = m.snapshot(0);
  CHECK_STR_EQ(s.base_name, "");
}

void test_mode_machine_invalid_kind_and_name() {
  ledwall::ModeMachine m = fresh_machine();
  ledwall::ModeKind parsed = ledwall::ModeKind::Idle;

  CHECK(!ledwall::mode_from_name("bogus", &parsed));
  CHECK(!ledwall::mode_from_name("", &parsed));
  CHECK(!ledwall::mode_from_name(nullptr, &parsed));
  CHECK(ledwall::mode_from_name("expression", &parsed));
  CHECK_EQ_INT(static_cast<int>(parsed), static_cast<int>(ledwall::ModeKind::Expression));

  // Defensive: out-of-range kind mutates nothing.
  const auto bogus = static_cast<ledwall::ModeKind>(200);
  CHECK(!m.apply_persistent(bogus, "x"));
  CHECK(!m.apply_transient(bogus, "x", 1000, 0));
  ledwall::ModeSnapshot s = m.snapshot(0);
  CHECK_EQ_INT(static_cast<int>(s.base_mode), static_cast<int>(ledwall::ModeKind::Idle));
  CHECK(!s.transient_active);
  CHECK(!m.dirty());
}
