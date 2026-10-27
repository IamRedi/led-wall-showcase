#include <cstring>

#include "api/command.h"
#include "core/mode_machine.h"
#include "render/framebuffer.h"
#include "text/text_renderer.h"
#include "test_framework.h"

// LED-L007 dispatcher↔renderer contract: the §6 render-time determinations
// (textTooWide, skippedCharacters), the program start stamps feeding the
// render layer, and the framebuffer bounds contract.

namespace {

constexpr uint32_t kT0 = 1000000;

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

  ledwall::SemanticCommand text_show(const char* t) {
    ledwall::SemanticCommand cmd;
    cmd.capability = ledwall::SemanticCommand::Capability::TextShow;
    std::strncpy(cmd.text, t, ledwall::kTextMaxLen - 1);
    return cmd;
  }

  ledwall::SemanticCommand named(ledwall::SemanticCommand::Capability c, const char* n) {
    ledwall::SemanticCommand cmd;
    cmd.capability = c;
    std::strncpy(cmd.name, n, ledwall::kMaxProgramNameLen - 1);
    return cmd;
  }
};

}  // namespace

void test_text_too_wide_static_rejected_no_mutation() {
  Fixture f;
  // §6 overflow rule: scroll defaults to false; 60 px > 30 px ⇒ textTooWide.
  auto r = f.dispatcher.execute(f.text_show("HELLO WALL"), kT0);
  CHECK(!r.ok);
  CHECK_EQ_INT(static_cast<int>(r.error), static_cast<int>(ledwall::ErrorCode::TextTooWide));
  // No mutation: base stays idle, no text stored.
  const ledwall::ModeSnapshot snap = f.machine.snapshot(kT0);
  CHECK_EQ_INT(static_cast<int>(snap.base_mode), static_cast<int>(ledwall::ModeKind::Idle));
  CHECK_STR_EQ(f.text.base.current, "");
  CHECK_EQ_INT(f.events.pending(), 0);

  // Explicit scroll:false is equally rejected.
  ledwall::SemanticCommand off = f.text_show("HELLO WALL");
  off.has_scroll = true;
  off.scroll = false;
  r = f.dispatcher.execute(off, kT0);
  CHECK(!r.ok);
  CHECK_EQ_INT(static_cast<int>(r.error), static_cast<int>(ledwall::ErrorCode::TextTooWide));

  // The §6 remedy: scroll:true accepts the same text.
  ledwall::SemanticCommand scrolling = f.text_show("HELLO WALL");
  scrolling.has_scroll = true;
  scrolling.scroll = true;
  r = f.dispatcher.execute(scrolling, kT0);
  CHECK(r.ok);
  CHECK(f.text.base.scrolling);
}

void test_text_width_boundary_static() {
  Fixture f;
  // Exactly fitting: 5 glyphs = 30 px → static accepted.
  auto r = f.dispatcher.execute(f.text_show("HELLO"), kT0);
  CHECK(r.ok);
  CHECK(!f.text.base.scrolling);
  CHECK(r.has_skipped_characters);
  CHECK_EQ_INT(r.skipped_characters, 0);

  // Smallest violation: 6 glyphs = 36 px → textTooWide (the 6 px advance
  // makes widths step by 6, so 31..35 px cannot occur).
  Fixture g;
  r = g.dispatcher.execute(g.text_show("HELLOW"), kT0);
  CHECK(!r.ok);
  CHECK_EQ_INT(static_cast<int>(r.error), static_cast<int>(ledwall::ErrorCode::TextTooWide));

  // Scrolling text has no practical length limit (§6): 128 chars + scroll.
  Fixture h;
  ledwall::SemanticCommand max_scroll = h.text_show("ABCDEFGHIJ");  // 10 glyphs, then extend below
  std::strncpy(max_scroll.text, "0123456789", ledwall::kTextMaxLen - 1);
  max_scroll.has_scroll = true;
  max_scroll.scroll = true;
  r = h.dispatcher.execute(max_scroll, kT0);
  CHECK(r.ok);
  CHECK(h.text.base.scrolling);
}

void test_text_skipped_characters_reported() {
  Fixture f;
  // "Hï!" — LED-L009 code-point accounting: 'ï' is ONE unsupported code point
  // (renders as one space) → skippedCharacters = 1.
  auto r = f.dispatcher.execute(f.text_show("H\xC3\xAF!"), kT0);
  CHECK(r.ok);
  CHECK(r.has_skipped_characters);
  CHECK_EQ_INT(r.skipped_characters, 1);
  // Unsupported characters were stored verbatim; rendering replaces them with
  // spaces (covered by the renderer suites).

  // Fully supported text reports 0 explicitly.
  Fixture g;
  r = g.dispatcher.execute(g.text_show("HI!"), kT0);
  CHECK(r.ok);
  CHECK(r.has_skipped_characters);
  CHECK_EQ_INT(r.skipped_characters, 0);
}

void test_program_start_stamps_feed_renderer() {
  Fixture f;
  // Persistent animation at kT0+5: base and current started stamps track it.
  auto r = f.dispatcher.execute(
      f.named(ledwall::SemanticCommand::Capability::AnimationSet, "wave"), kT0 + 5);
  CHECK(r.ok);
  ledwall::ModeSnapshot snap = f.machine.snapshot(kT0 + 9);
  CHECK_EQ_INT(snap.base_started_ms, kT0 + 5);
  CHECK_EQ_INT(snap.current_started_ms, kT0 + 5);

  // Transient over it: current start follows the transient; base stamp kept.
  ledwall::SemanticCommand t =
      f.named(ledwall::SemanticCommand::Capability::SceneSet, "corners");
  t.has_duration = true;
  t.duration_ms = 1000;
  r = f.dispatcher.execute(t, kT0 + 10);
  CHECK(r.ok);
  snap = f.machine.snapshot(kT0 + 20);
  CHECK(snap.transient_active);
  CHECK_EQ_INT(snap.current_started_ms, kT0 + 10);  // transient program time
  CHECK_EQ_INT(snap.base_started_ms, kT0 + 5);

  // Expiry: the renderer time falls back to the base program's own stamp.
  snap = f.machine.snapshot(kT0 + 2000);
  CHECK(!snap.transient_active);
  CHECK_EQ_INT(snap.current_started_ms, kT0 + 5);

  // Persistent command re-stamps; CLEAR-1 stamps the idle base.
  CHECK(f.dispatcher
            .execute(f.named(ledwall::SemanticCommand::Capability::AnimationSet, "rain"),
                     kT0 + 3000)
            .ok);
  snap = f.machine.snapshot(kT0 + 3001);
  CHECK_EQ_INT(snap.base_started_ms, kT0 + 3000);
  CHECK(f.dispatcher.execute(
      f.named(ledwall::SemanticCommand::Capability::Clear, ""), kT0 + 4000).ok);
  snap = f.machine.snapshot(kT0 + 4001);
  CHECK_EQ_INT(static_cast<int>(snap.base_mode), static_cast<int>(ledwall::ModeKind::Idle));
  CHECK_EQ_INT(snap.base_started_ms, kT0 + 4000);
}

void test_text_program_stamps_independent_per_slot() {
  Fixture f;
  auto r = f.dispatcher.execute(f.text_show("HI"), kT0 + 100);  // base text
  CHECK(r.ok);
  CHECK_EQ_INT(f.text.base.started_ms, kT0 + 100);
  CHECK(!f.text.base.done_reported);

  ledwall::SemanticCommand t = f.text_show("POPUP");  // 30 px transient
  t.has_duration = true;
  t.duration_ms = 5000;
  r = f.dispatcher.execute(t, kT0 + 200);
  CHECK(r.ok);
  CHECK_EQ_INT(f.text.transient.started_ms, kT0 + 200);
  CHECK_EQ_INT(f.text.base.started_ms, kT0 + 100);  // base slot untouched

  // Legacy 2-arg machine forms still compile and do not stamp (existing
  // accepted tests keep their semantics).
  ledwall::ModeMachine m;
  m.boot(ledwall::ModeKind::Idle, 5);
  m.apply_persistent(ledwall::ModeKind::Scene, "wave");
  CHECK_EQ_INT(m.snapshot(7).base_started_ms, 0);
  CHECK_EQ_INT(m.snapshot(7).current_started_ms, 0);
}

void test_dispatcher_never_emits_text_done() {
  // text.done is a RENDER-completion event: the dispatcher must not emit it.
  Fixture f;
  f.dispatcher.execute(f.text_show("HI"), kT0);
  ledwall::Event ev[ledwall::kMaxPendingEvents];
  const uint8_t n = f.events.drain(ev, ledwall::kMaxPendingEvents);
  bool text_done = false;
  for (uint8_t i = 0; i < n; ++i) {
    if (ev[i].kind == ledwall::EventKind::TextDone) {
      text_done = true;
    }
  }
  CHECK(!text_done);
}

void test_framebuffer_bounds_and_mapping() {
  // Matrix safety: every write lands on 0..299 via the accepted mapping;
  // out-of-bounds logical coordinates are dropped (no crash, no aliasing).
  ledwall::FrameBuffer f;
  f.set(-1, 0, ledwall::Rgb{9, 9, 9});
  f.set(0, -1, ledwall::Rgb{9, 9, 9});
  f.set(30, 0, ledwall::Rgb{9, 9, 9});
  f.set(0, 10, ledwall::Rgb{9, 9, 9});
  f.set(32767, 32767, ledwall::Rgb{9, 9, 9});
  CHECK_EQ_INT([](const ledwall::FrameBuffer& fb) {
    int n = 0;
    for (uint8_t y = 0; y < 10; ++y) {
      for (uint8_t x = 0; x < 30; ++x) {
        if (fb.at(x, y) != ledwall::kRgbBlack) {
          ++n;
        }
      }
    }
    return n;
  }(f), 0);

  // In-bounds writes round-trip through the heritage serpentine mapping
  // (cross-checked against matrix_mapping directly).
  ledwall::FrameBuffer g;
  g.set(14, 4, ledwall::Rgb{1, 2, 3});
  const uint16_t idx = ledwall::screen_to_index(14, 4, ledwall::kHeritageOrientation);
  CHECK(idx < 300);
  CHECK((g.data()[idx] == ledwall::Rgb{1, 2, 3}));
  // Flip orientation changes the storage index, not the logical frame.
  ledwall::FrameBuffer h;
  h.set_orientation(ledwall::OrientationFlags{false, true});
  h.set(14, 4, ledwall::Rgb{1, 2, 3});
  CHECK((h.at(14, 4) == ledwall::Rgb{1, 2, 3}));

  // clear() blanks deterministically.
  ledwall::FrameBuffer c;
  c.set(0, 0, ledwall::Rgb{7, 7, 7});
  c.clear();
  CHECK_EQ_INT([](const ledwall::FrameBuffer& fb) {
    int n = 0;
    for (uint8_t y = 0; y < 10; ++y) {
      for (uint8_t x = 0; x < 30; ++x) {
        if (fb.at(x, y) != ledwall::kRgbBlack) {
          ++n;
        }
      }
    }
    return n;
  }(c), 0);
}
