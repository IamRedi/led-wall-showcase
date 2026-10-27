#include <cstring>

#include "core/brightness.h"
#include "core/config.h"
#include "core/public_state.h"
#include "test_framework.h"

// R-B02 regression tests: the public state must read the mode machine on the
// SAME monotonic now as the tick loop; uptime_seconds is an independently
// derived display field. Expected remainingMs values are computed from the §11
// timeline (creation + duration - elapsed), never from the implementation.

namespace {

constexpr uint32_t kBootWall = 10000;      // device booted at wall 10 000 ms
constexpr uint32_t kCreateWall = 20000;    // transient created at wall 20 000 ms
constexpr uint32_t kDuration = 5000;

ledwall::ModeMachine machine_with_transient(uint32_t create_wall) {
  ledwall::ModeMachine m;
  m.boot(ledwall::ModeKind::Animation, 1);
  m.apply_persistent(ledwall::ModeKind::Animation, "wave");
  m.apply_transient(ledwall::ModeKind::Expression, "happy", kDuration, create_wall);
  return m;
}

ledwall::Brightness heritage_brightness() {
  ledwall::Brightness b;
  b.configure(40, 40);
  return b;
}

}  // namespace

void test_public_state_remaining_independent_of_boot_offset() {
  // Probe from the failed review: boot 10 000, created 20 000, queried at
  // 21 000 (uptime 11 000) → true remaining 4 000 (pre-repair wiring reported
  // 14 000 by feeding uptime into the machine).
  ledwall::ModeMachine m = machine_with_transient(kCreateWall);
  const ledwall::Brightness b = heritage_brightness();
  const ledwall::Warnings w{};
  const ledwall::PublicState s =
      ledwall::build_public_state(m, b, ledwall::TextState{}, w, kCreateWall + 1000, 11);
  CHECK(s.transient_active);
  CHECK_EQ_INT(s.transient_remaining_ms, 4000);
  CHECK_EQ_INT(static_cast<int>(s.current_mode), static_cast<int>(ledwall::ModeKind::Expression));
  CHECK_EQ_INT(static_cast<int>(s.base_mode), static_cast<int>(ledwall::ModeKind::Animation));
  CHECK_STR_EQ(s.base_name, "wave");
  CHECK_EQ_INT(s.uptime_seconds, 11);  // display field, not timing input
}

void test_public_state_remaining_independent_of_absolute_wall() {
  // Same timeline anchored at a huge wall value: the remainingMs result must
  // not depend on the absolute boot/wall magnitude.
  ledwall::ModeMachine m = machine_with_transient(4000000000u);
  const ledwall::PublicState s =
      ledwall::build_public_state(m, heritage_brightness(), ledwall::TextState{},
                                  ledwall::Warnings{}, 4000000000u + 1000, 1);
  CHECK(s.transient_active);
  CHECK_EQ_INT(s.transient_remaining_ms, 4000);
}

void test_public_state_after_expiry() {
  ledwall::ModeMachine m = machine_with_transient(kCreateWall);
  const ledwall::PublicState s =
      ledwall::build_public_state(m, heritage_brightness(), ledwall::TextState{},
                                  ledwall::Warnings{}, kCreateWall + kDuration, 15);
  CHECK(!s.transient_active);
  CHECK_EQ_INT(s.transient_remaining_ms, 0);
  CHECK_EQ_INT(static_cast<int>(s.current_mode), static_cast<int>(ledwall::ModeKind::Animation));
  CHECK_STR_EQ(s.current_name, "wave");
}

void test_public_state_rollover() {
  // Transient created near the uint32 ceiling; deadline wraps to 904.
  ledwall::ModeMachine m = machine_with_transient(0xFFFFF000u);
  const ledwall::Warnings w{};
  const ledwall::PublicState before =
      ledwall::build_public_state(m, heritage_brightness(), ledwall::TextState{}, w, 300, 1);
  CHECK(before.transient_active);
  CHECK_EQ_INT(before.transient_remaining_ms, 604);  // 904 - 300, wrap-safe

  const ledwall::PublicState after =
      ledwall::build_public_state(m, heritage_brightness(), ledwall::TextState{}, w, 904, 1);
  CHECK(!after.transient_active);
  CHECK_EQ_INT(after.transient_remaining_ms, 0);
  CHECK_EQ_INT(static_cast<int>(after.current_mode), static_cast<int>(ledwall::ModeKind::Animation));
}

void test_public_state_frozen_spec_fields() {
  ledwall::ModeMachine m = machine_with_transient(kCreateWall);
  ledwall::Brightness b;
  b.configure(40, 40);
  ledwall::Warnings w{};
  ledwall::add_warning(&w, "ignored unknown config key 'widget.foo'");

  const ledwall::PublicState s = ledwall::build_public_state(m, b, ledwall::TextState{}, w, kCreateWall, 10);
  CHECK(s.available);
  CHECK_STR_EQ(s.device_type, "visual_display_v1");
  CHECK_EQ_INT(s.protocol_version, 1);
  CHECK_EQ_INT(s.matrix_width, 30);
  CHECK_EQ_INT(s.matrix_height, 10);
  CHECK_EQ_INT(s.brightness_current, 40);  // heritage default, ceiling-clamped
  CHECK_EQ_INT(s.brightness_max, 40);
  CHECK(!s.spectrum_available);  // mic unconfirmed → capability omitted (§14)
  CHECK(!s.spectrum_active);
  CHECK_EQ_INT(s.warning_count, 1);
  CHECK_STR_EQ(s.warnings[0], "ignored unknown config key 'widget.foo'");
  CHECK_EQ_INT(s.clients_count, 0);
  CHECK_EQ_INT(s.uptime_seconds, 10);
}

void test_public_state_text_reporting_by_mode() {
  // §4: text {current, scrolling} applies only when text is the program; a
  // text transient overrides base text; expiry falls back without mutation.
  ledwall::ModeMachine m;
  m.boot(ledwall::ModeKind::Idle, 1000);
  const ledwall::Brightness b = heritage_brightness();
  const ledwall::Warnings w{};

  ledwall::TextState text;
  std::strncpy(text.base.current, "BASE TEXT", ledwall::kTextMaxLen - 1);
  text.base.scrolling = true;
  text.base.speed_ms_per_px = 200;

  // Base text: reported while baseMode == text.
  m.apply_persistent(ledwall::ModeKind::Text, "");
  ledwall::PublicState s = ledwall::build_public_state(m, b, text, w, 2000, 1);
  CHECK_STR_EQ(s.text_current, "BASE TEXT");
  CHECK(s.text_scrolling);
  CHECK_EQ_INT(s.text_speed_ms_per_px, 200);

  // Transient text overrides the reported content until it expires.
  m.apply_transient(ledwall::ModeKind::Text, "", 5000, 2000);
  std::strncpy(text.transient.current, "POPUP", ledwall::kTextMaxLen - 1);
  text.transient.scrolling = false;
  text.transient.speed_ms_per_px = 140;
  s = ledwall::build_public_state(m, b, text, w, 3000, 2);
  CHECK(s.transient_active);
  CHECK_STR_EQ(s.text_current, "POPUP");
  CHECK(!s.text_scrolling);

  // Expiry: reported text returns to the base slot with zero mutation.
  s = ledwall::build_public_state(m, b, text, w, 7000, 6);
  CHECK(!s.transient_active);
  CHECK_STR_EQ(s.text_current, "BASE TEXT");
  CHECK(s.text_scrolling);

  // Non-text base: no text reported even though slots hold content.
  m.apply_persistent(ledwall::ModeKind::Scene, "wave");
  s = ledwall::build_public_state(m, b, text, w, 8000, 7);
  CHECK_EQ_INT(static_cast<int>(s.base_mode), static_cast<int>(ledwall::ModeKind::Scene));
  CHECK_STR_EQ(s.text_current, "");
  CHECK(!s.text_scrolling);
}
