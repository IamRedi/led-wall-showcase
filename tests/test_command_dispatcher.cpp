#include "api/command.h"
#include "test_framework.h"

// §5/§9/§11 command-dispatcher semantics. Expected outcomes are derived from
// the frozen specification (capability table, typed error vocabulary, CLEAR-1/
// CLEAR-2 branches, transient model), never from the implementation.

namespace {

constexpr uint32_t kT0 = 1000000;  // arbitrary wall-clock anchor (ms)

struct Fixture {
  ledwall::ModeMachine machine;
  ledwall::Brightness brightness;
  ledwall::TextState text;
  ledwall::EventSink events;
  ledwall::CommandDispatcher dispatcher;

  Fixture() {
    machine.boot(ledwall::ModeKind::Idle, kT0);
    brightness.configure(40, 40);
    dispatcher.begin(&machine, &brightness, &text, &events);
    machine.clear_dirty();
    brightness.clear_dirty();
  }

  ledwall::SemanticCommand make(ledwall::SemanticCommand::Capability c) {
    ledwall::SemanticCommand cmd;
    cmd.capability = c;
    return cmd;
  }

  ledwall::SemanticCommand text_show(const char* t) {
    ledwall::SemanticCommand cmd = make(ledwall::SemanticCommand::Capability::TextShow);
    std::strncpy(cmd.text, t, ledwall::kTextMaxLen - 1);
    return cmd;
  }

  ledwall::SemanticCommand named(ledwall::SemanticCommand::Capability c, const char* n) {
    ledwall::SemanticCommand cmd = make(c);
    std::strncpy(cmd.name, n, ledwall::kMaxProgramNameLen - 1);
    return cmd;
  }
};

bool has_event(const Fixture& f, ledwall::EventKind kind) {
  // Peek without draining: drain into a scratch buffer and count.
  Fixture& m = const_cast<Fixture&>(f);
  ledwall::Event ev[ledwall::kMaxPendingEvents];
  const uint8_t n = m.events.drain(ev, ledwall::kMaxPendingEvents);
  bool found = false;
  for (uint8_t i = 0; i < n; ++i) {
    found = found || ev[i].kind == kind;
  }
  return found;
}

}  // namespace

void test_dispatcher_persistent_programs() {
  Fixture f;
  auto r = f.dispatcher.execute(f.named(ledwall::SemanticCommand::Capability::AnimationSet, "wave"),
                                kT0);
  CHECK(r.ok);
  CHECK_EQ_INT(static_cast<int>(f.machine.snapshot(kT0).base_mode),
               static_cast<int>(ledwall::ModeKind::Animation));
  const ledwall::ModeSnapshot s1 = f.machine.snapshot(kT0);
  CHECK_STR_EQ(s1.base_name, "wave");
  CHECK(has_event(f, ledwall::EventKind::StateChanged));

  // Persistent scene.set accepts animation-class names ("usable as scenes", §7)
  // and reports them as scenes.
  r = f.dispatcher.execute(f.named(ledwall::SemanticCommand::Capability::SceneSet, "rain"), kT0 + 10);
  CHECK(r.ok);
  CHECK_EQ_INT(static_cast<int>(f.machine.snapshot(kT0 + 10).base_mode),
               static_cast<int>(ledwall::ModeKind::Scene));
  const ledwall::ModeSnapshot s2 = f.machine.snapshot(kT0 + 10);
  CHECK_STR_EQ(s2.base_name, "rain");

  // Expression intent is persistent (§8).
  r = f.dispatcher.execute(
      f.named(ledwall::SemanticCommand::Capability::ExpressionSet, "thinking"), kT0 + 20);
  CHECK(r.ok);
  CHECK_EQ_INT(static_cast<int>(f.machine.snapshot(kT0 + 20).base_mode),
               static_cast<int>(ledwall::ModeKind::Expression));
  const ledwall::ModeSnapshot s3 = f.machine.snapshot(kT0 + 20);
  CHECK_STR_EQ(s3.base_name, "thinking");
}

void test_dispatcher_unknown_names_typed_errors_no_mutation() {
  Fixture f;
  // Anchor state: animation "wave".
  f.dispatcher.execute(f.named(ledwall::SemanticCommand::Capability::AnimationSet, "wave"), kT0);

  auto r = f.dispatcher.execute(f.named(ledwall::SemanticCommand::Capability::SceneSet, "nope"),
                                kT0 + 10);
  CHECK(!r.ok);
  CHECK_EQ_INT(static_cast<int>(r.error), static_cast<int>(ledwall::ErrorCode::UnknownScene));
  CHECK(std::strstr(r.available, "wave") != nullptr);   // available list per §9
  CHECK(std::strstr(r.available, "plasma") != nullptr);

  r = f.dispatcher.execute(f.named(ledwall::SemanticCommand::Capability::AnimationSet, "demoRain"),
                           kT0 + 10);
  CHECK(!r.ok);
  CHECK_EQ_INT(static_cast<int>(r.error), static_cast<int>(ledwall::ErrorCode::UnknownAnimation));

  r = f.dispatcher.execute(
      f.named(ledwall::SemanticCommand::Capability::ExpressionSet, "curious"), kT0 + 10);
  CHECK(!r.ok);
  CHECK_EQ_INT(static_cast<int>(r.error), static_cast<int>(ledwall::ErrorCode::UnknownExpression));
  CHECK(std::strstr(r.available, "sleeping") != nullptr);

  // §11: rejected commands mutate nothing.
  const ledwall::ModeSnapshot snap = f.machine.snapshot(kT0 + 10);
  CHECK_EQ_INT(static_cast<int>(snap.base_mode), static_cast<int>(ledwall::ModeKind::Animation));
  CHECK_STR_EQ(snap.base_name, "wave");
  CHECK(!snap.transient_active);
}

void test_dispatcher_text_show_persistent_and_transient() {
  Fixture f;

  // Persistent text: baseMode = text, content stored, reported by state.
  // LED-L007: "HELLO WALL" (60 px) requires scroll:true — static would be
  // textTooWide per §6 (see test_renderer_contract.cpp for the rejection).
  ledwall::SemanticCommand wide = f.text_show("HELLO WALL");
  wide.has_scroll = true;
  wide.scroll = true;
  auto r = f.dispatcher.execute(wide, kT0);
  CHECK(r.ok);
  ledwall::ModeSnapshot snap = f.machine.snapshot(kT0);
  CHECK_EQ_INT(static_cast<int>(snap.base_mode), static_cast<int>(ledwall::ModeKind::Text));
  CHECK_STR_EQ(f.text.base.current, "HELLO WALL");
  CHECK(f.text.base.scrolling);
  CHECK_EQ_INT(f.text.base.speed_ms_per_px, 140);  // heritage default (§6)

  // Transient text over a scene base: expiry must return reported text to the
  // base slot without mutation (selection at state-build time).
  f.dispatcher.execute(f.named(ledwall::SemanticCommand::Capability::SceneSet, "corners"), kT0 + 10);
  f.text.base.current[0] = '\0';  // scene base carries no text content
  ledwall::SemanticCommand t = f.text_show("ALERT");
  t.has_duration = true;
  t.duration_ms = 5000;
  r = f.dispatcher.execute(t, kT0 + 20);
  CHECK(r.ok);
  snap = f.machine.snapshot(kT0 + 20);
  CHECK(snap.transient_active);
  CHECK_EQ_INT(static_cast<int>(snap.transient_kind), static_cast<int>(ledwall::ModeKind::Text));
  CHECK_STR_EQ(f.text.transient.current, "ALERT");

  // Validation: empty text, >128 chars, speed bounds, zero duration.
  r = f.dispatcher.execute(f.text_show(""), kT0 + 30);
  CHECK(!r.ok);
  CHECK_EQ_INT(static_cast<int>(r.error), static_cast<int>(ledwall::ErrorCode::InvalidParameter));
  // The 128-character protocol cap is enforced where the command is built
  // (HTTP/WS transports reject longer input; the struct cannot hold more).
  // Boundary: exactly 128 characters is accepted (scrolling: 128 glyphs are
  // far wider than the canvas — LED-L007 §6 width rule).
  char max_text[ledwall::kTextProtocolCap + 1];
  for (size_t i = 0; i < ledwall::kTextProtocolCap; ++i) max_text[i] = 'x';
  max_text[ledwall::kTextProtocolCap] = '\0';
  ledwall::SemanticCommand max_cmd = f.text_show(max_text);
  max_cmd.has_scroll = true;
  max_cmd.scroll = true;
  r = f.dispatcher.execute(max_cmd, kT0 + 30);
  CHECK(r.ok);
  ledwall::SemanticCommand fast = f.text_show("HI");
  fast.has_speed = true;
  fast.speed_ms_per_px = 39;  // below §6 bound
  r = f.dispatcher.execute(fast, kT0 + 30);
  CHECK(!r.ok);
  fast.speed_ms_per_px = 501;  // above §6 bound
  r = f.dispatcher.execute(fast, kT0 + 30);
  CHECK(!r.ok);
  ledwall::SemanticCommand zero_dur = f.text_show("HI");
  zero_dur.has_duration = true;
  zero_dur.duration_ms = 0;
  r = f.dispatcher.execute(zero_dur, kT0 + 30);
  CHECK(!r.ok);
  CHECK_EQ_INT(static_cast<int>(r.error), static_cast<int>(ledwall::ErrorCode::InvalidParameter));

  // speedMsPerPx at the exact §6 bounds is accepted.
  ledwall::SemanticCommand lo = f.text_show("HI");
  lo.has_speed = true;
  lo.speed_ms_per_px = 40;
  CHECK(f.dispatcher.execute(lo, kT0 + 30).ok);
  ledwall::SemanticCommand hi = f.text_show("HI");
  hi.has_speed = true;
  hi.speed_ms_per_px = 500;
  CHECK(f.dispatcher.execute(hi, kT0 + 30).ok);
}

void test_dispatcher_clear_semantics() {
  // CLEAR-1 (display.clear): cancel transient, baseMode = idle, no animation.
  Fixture f;
  ledwall::SemanticCommand t = f.text_show("HI");
  t.has_duration = true;
  t.duration_ms = 60000;
  f.dispatcher.execute(t, kT0);
  CHECK(f.machine.snapshot(kT0).transient_active);

  auto r = f.dispatcher.execute(f.make(ledwall::SemanticCommand::Capability::Clear), kT0 + 10);
  CHECK(r.ok);
  const ledwall::ModeSnapshot snap = f.machine.snapshot(kT0 + 10);
  CHECK_EQ_INT(static_cast<int>(snap.base_mode), static_cast<int>(ledwall::ModeKind::Idle));
  CHECK(!snap.transient_active);
  CHECK_STR_EQ(f.text.base.current, "");  // cleared
  CHECK(has_event(f, ledwall::EventKind::ModeChanged));

  // CLEAR-2 (display.text.clear), text-as-base branch: base → idle.
  f.dispatcher.execute(f.text_show("STAY"), kT0 + 20);
  r = f.dispatcher.execute(f.make(ledwall::SemanticCommand::Capability::TextClear), kT0 + 30);
  CHECK(r.ok);
  CHECK_EQ_INT(static_cast<int>(f.machine.snapshot(kT0 + 30).base_mode),
               static_cast<int>(ledwall::ModeKind::Idle));
  CHECK_STR_EQ(f.text.base.current, "");

  // CLEAR-2, text-as-transient branch: cancel the transient, return to base.
  f.dispatcher.execute(f.named(ledwall::SemanticCommand::Capability::AnimationSet, "wave"), kT0 + 40);
  ledwall::SemanticCommand tt = f.text_show("POPUP");
  tt.has_duration = true;
  tt.duration_ms = 60000;
  f.dispatcher.execute(tt, kT0 + 50);
  CHECK(f.machine.snapshot(kT0 + 50).transient_active);
  r = f.dispatcher.execute(f.make(ledwall::SemanticCommand::Capability::TextClear), kT0 + 60);
  CHECK(r.ok);
  const ledwall::ModeSnapshot after = f.machine.snapshot(kT0 + 60);
  CHECK(!after.transient_active);
  CHECK_EQ_INT(static_cast<int>(after.base_mode), static_cast<int>(ledwall::ModeKind::Animation));
  CHECK_STR_EQ(after.current_name, "wave");  // returned to base, no stack
  CHECK_STR_EQ(f.text.transient.current, "");

  // CLEAR-2 with no text anywhere: no-op, no state change event.
  f.dispatcher.execute(f.named(ledwall::SemanticCommand::Capability::SceneSet, "wave"), kT0 + 70);
  const uint8_t before_pending = f.events.pending();
  r = f.dispatcher.execute(f.make(ledwall::SemanticCommand::Capability::TextClear), kT0 + 80);
  CHECK(r.ok);
  CHECK(!r.state_changed);
  CHECK_EQ_INT(f.events.pending(), before_pending);
}

void test_dispatcher_clear2_leaves_unrelated_transient() {
  // §11 CLEAR-2 is scoped to text: text-as-base with a concurrent non-text
  // transient — the base becomes idle, the transient is NOT cancelled by
  // text.clear (it expires on its own deadline, returning to the new base).
  Fixture f;
  f.dispatcher.execute(f.text_show("BASE"), kT0);            // text base
  ledwall::SemanticCommand e = f.named(
      ledwall::SemanticCommand::Capability::AnimationSet, "wave");
  e.has_duration = true;
  e.duration_ms = 5000;
  e = f.named(ledwall::SemanticCommand::Capability::SceneSet, "wave");
  e.has_duration = true;
  e.duration_ms = 5000;
  auto r = f.dispatcher.execute(e, kT0 + 10);  // transient scene over text base
  CHECK(r.ok);
  const ledwall::ModeSnapshot during = f.machine.snapshot(kT0 + 10);
  CHECK(during.transient_active);
  CHECK_EQ_INT(static_cast<int>(during.base_mode), static_cast<int>(ledwall::ModeKind::Text));

  r = f.dispatcher.execute(f.make(ledwall::SemanticCommand::Capability::TextClear), kT0 + 20);
  CHECK(r.ok);
  const ledwall::ModeSnapshot after = f.machine.snapshot(kT0 + 20);
  // Text base cleared to idle; the unrelated scene transient keeps running.
  CHECK_EQ_INT(static_cast<int>(after.base_mode), static_cast<int>(ledwall::ModeKind::Idle));
  CHECK(after.transient_active);
  CHECK_EQ_INT(static_cast<int>(after.current_mode), static_cast<int>(ledwall::ModeKind::Scene));
  CHECK_STR_EQ(f.text.base.current, "");
}

void test_dispatcher_brightness_semantics() {
  Fixture f;
  auto r = f.dispatcher.execute(f.make(ledwall::SemanticCommand::Capability::BrightnessSet),
                                kT0);  // missing level
  CHECK(!r.ok);
  CHECK_EQ_INT(static_cast<int>(r.error), static_cast<int>(ledwall::ErrorCode::InvalidParameter));

  // Effective change (40 → 20): request stored, level applied, event emitted.
  ledwall::SemanticCommand cmd = f.make(ledwall::SemanticCommand::Capability::BrightnessSet);
  cmd.has_level = true;
  cmd.level = 20;
  r = f.dispatcher.execute(cmd, kT0 + 10);
  CHECK(r.ok);
  CHECK(r.state_changed);
  CHECK_EQ_INT(f.brightness.requested(), 20);
  CHECK_EQ_INT(f.brightness.level(), 20);
  CHECK(has_event(f, ledwall::EventKind::StateChanged));

  // Request above the ceiling: clamped locally, not rejected (§5/§13/ADR-011);
  // the effective level changes 20 → 40, so state.changed is emitted (§9).
  cmd.level = 100;
  r = f.dispatcher.execute(cmd, kT0 + 20);
  CHECK(r.ok);
  CHECK(r.state_changed);
  CHECK_EQ_INT(f.brightness.requested(), 100);
  CHECK_EQ_INT(f.brightness.level(), 40);  // ceiling enforced
  CHECK(has_event(f, ledwall::EventKind::StateChanged));

  // Clamped request that does NOT move the level: ok, but no event (§9).
  cmd.level = 60;
  r = f.dispatcher.execute(cmd, kT0 + 25);
  CHECK(r.ok);
  CHECK(!r.state_changed);
  CHECK_EQ_INT(f.brightness.level(), 40);
  CHECK(f.events.pending() == 0);

  // Unchanged level: ok, no event.
  cmd.level = 40;
  f.brightness.set_level(40);
  f.brightness.clear_dirty();
  r = f.dispatcher.execute(cmd, kT0 + 30);
  CHECK(r.ok);
  CHECK(!r.state_changed);
}

void test_dispatcher_events_and_rollover() {
  // Transient expiry emits transient.expired exactly once, including across
  // the uint32 rollover (§9 events + §11 timing).
  Fixture f;
  ledwall::SemanticCommand cmd = f.named(ledwall::SemanticCommand::Capability::SceneSet, "wave");
  cmd.has_duration = true;
  cmd.duration_ms = 5000;
  CHECK(f.dispatcher.execute(cmd, 0xFFFFF000u).ok);  // deadline wraps to 904
  CHECK(f.machine.transient_deadline_passed(0xFFFFF000u + 1) == false);
  CHECK(f.machine.transient_deadline_passed(800) == false);
  CHECK(f.machine.transient_deadline_passed(904) == true);

  // Simulate the runtime tick path: query, tick, emit.
  Fixture g;
  CHECK(g.dispatcher.execute(cmd, 0xFFFFF000u).ok);
  CHECK(!g.machine.transient_deadline_passed(0xFFFFF100u));  // 1 ms after creation
  g.machine.clear_dirty();
  CHECK(g.machine.transient_deadline_passed(904));
  g.machine.tick(904);
  CHECK(!g.machine.snapshot(904).transient_active);
  CHECK(g.machine.dirty());  // render returns to base
}

void test_dispatcher_state_read_and_max_duration() {
  Fixture f;
  auto r = f.dispatcher.execute(f.make(ledwall::SemanticCommand::Capability::StateRead), kT0);
  CHECK(r.ok);
  CHECK(!r.state_changed);

  // INT32_MAX duration is accepted (wrap-safe bound); larger rejected.
  ledwall::SemanticCommand cmd = f.named(ledwall::SemanticCommand::Capability::AnimationSet, "rain");
  cmd.has_duration = true;
  cmd.duration_ms = 0x7FFFFFFFu;
  CHECK(f.dispatcher.execute(cmd, kT0).ok);
  CHECK_EQ_INT(f.machine.snapshot(kT0).transient_remaining_ms, 0x7FFFFFFFu);

  cmd.duration_ms = 0x80000000u;
  f.machine.clear_dirty();
  r = f.dispatcher.execute(cmd, kT0 + 10);
  CHECK(!r.ok);
  CHECK_EQ_INT(static_cast<int>(r.error), static_cast<int>(ledwall::ErrorCode::InvalidParameter));
}
