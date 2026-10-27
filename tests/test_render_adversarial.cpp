#include <cstring>

#include <initializer_list>

#include "effects/animation_renderers.h"
#include "render/color.h"
#include "render/framebuffer.h"
#include "render/renderer.h"
#include "text/text_renderer.h"
#include "test_framework.h"

// LED-L007 adversarial renderer pass (C-L007-7): hostile inputs to the render
// layer must behave boundedly and deterministically — division guards, wrap
// extremes, buffer reuse, orientation combinations, long-running counters.

namespace {

constexpr uint8_t kW = 30;
constexpr uint8_t kH = 10;

bool frame_equals(const ledwall::FrameBuffer& a, const ledwall::FrameBuffer& b) {
  return std::memcmp(a.data(), b.data(), 300 * sizeof(ledwall::Rgb)) == 0;
}

int count_lit(const ledwall::FrameBuffer& f) {
  int n = 0;
  for (uint8_t y = 0; y < kH; ++y) {
    for (uint8_t x = 0; x < kW; ++x) {
      if (f.at(x, y) != ledwall::kRgbBlack) {
        ++n;
      }
    }
  }
  return n;
}

}  // namespace

void test_adversarial_scroll_speed_zero_guard() {
  // The dispatcher clamps speed to 40..500, but the renderer must not divide
  // by zero for malformed render input: speed 0 degrades to 1 px/ms.
  // elapsed 123 → offset = 123 % 42 − 12 = 27 → H's first three columns clip
  // at the right canvas edge (7 + 1 + 1 = 9 visible px), the rest off-screen.
  ledwall::FrameBuffer f;
  ledwall::text::draw_scrolling(f, "HI", ledwall::Rgb{10, 10, 10}, 123, 0);
  CHECK_EQ_INT(count_lit(f), 9);
  CHECK((f.at(29, 5) != ledwall::kRgbBlack));
  // Deterministic under repetition.
  ledwall::FrameBuffer g;
  ledwall::text::draw_scrolling(g, "HI", ledwall::Rgb{10, 10, 10}, 123, 0);
  CHECK(frame_equals(f, g));
}

void test_adversarial_multibyte_skipped_accounting() {
  // LED-L009 code-point accounting: multi-byte sequences count ONCE per code
  // point (each unsupported code point renders as exactly one 6 px space);
  // invalid bytes still count one each (deterministic, bounded).
  CHECK_EQ_INT(ledwall::text::count_skipped_characters("\xE2\x82\xAC"), 1);       // €
  CHECK_EQ_INT(ledwall::text::count_skipped_characters("\xF0\x9F\x98\x80"), 1);   // emoji
  CHECK_EQ_INT(ledwall::text::count_skipped_characters("A\xE2\x82\xACZ"), 1);
  // Width is code-point based: "A€Z" is 3 code points → 18 px → fits.
  CHECK(ledwall::text::fits_static("A\xE2\x82\xACZ"));
  // 4-byte sequences: the lead byte is consumed alone (invalid here) — one
  // skipped per invalid byte, never a crash or unbounded loop.
  CHECK_EQ_INT(ledwall::text::count_skipped_characters("\xF0\x9F"), 2);
}

void test_adversarial_animation_time_wrap() {
  // Animation elapsed = now − started wraps safely: a wrapped pair renders
  // identically to the equivalent direct elapsed.
  const ledwall::effects::AnimationRenderer wave = ledwall::effects::animation_by_name("wave");
  ledwall::FrameBuffer wrapped, direct;
  wave(wrapped, 0x314u - 0xFFFFFF00u);  // wrapped elapsed = 1044
  wave(direct, 1044);
  CHECK(frame_equals(wrapped, direct));

  // Extreme counters stay bounded and deterministic.
  ledwall::FrameBuffer a, b;
  wave(a, 0xFFFFFFFFu);
  wave(b, 0xFFFFFFFFu);
  CHECK(frame_equals(a, b));
  ledwall::FrameBuffer p1, p2;
  ledwall::effects::render_plasma(p1, 0xFFFFFFF0u);
  ledwall::effects::render_plasma(p2, 0xFFFFFFF0u);
  CHECK(frame_equals(p1, p2));
}

void test_adversarial_buffer_reuse_leaves_no_residue() {
  // Renderer switch at a frame boundary: every renderer clears first, so a
  // busy frame followed by idle leaves a clean blank (never stale pixels).
  ledwall::FrameBuffer f;
  ledwall::renderer::RenderInput in;
  in.kind = ledwall::ModeKind::Animation;
  in.name = "plasma";
  in.now_ms = 4242;
  ledwall::renderer::render(f, in);
  CHECK_EQ_INT(count_lit(f), 300);

  in.kind = ledwall::ModeKind::Idle;
  in.name = "";
  ledwall::renderer::render(f, in);
  CHECK_EQ_INT(count_lit(f), 0);

  // Bright frame → static text: no plasma residue between glyphs.
  in.kind = ledwall::ModeKind::Animation;
  in.name = "plasma";
  ledwall::renderer::render(f, in);
  ledwall::TextProgram t;
  std::strncpy(t.current, "HI", sizeof(t.current) - 1);
  in.kind = ledwall::ModeKind::Text;
  in.name = "";
  in.text = &t;
  ledwall::renderer::render(f, in);
  CHECK_EQ_INT(count_lit(f), 28);
  CHECK(f.at(0, 0) == ledwall::kRgbBlack);
}

void test_adversarial_all_orientation_combinations() {
  // The logical frame is orientation-independent; only storage order changes.
  for (bool flip_odd : {false, true}) {
    for (bool flip_y : {false, true}) {
      ledwall::FrameBuffer f;
      f.set_orientation(ledwall::OrientationFlags{flip_odd, flip_y});
      ledwall::renderer::RenderInput in;
      in.kind = ledwall::ModeKind::Animation;
      in.name = "corners";
      in.now_ms = 0;
      ledwall::renderer::render(f, in);
      CHECK_EQ_INT(count_lit(f), 5);
      CHECK((f.at(0, 0) == ledwall::Rgb{255, 0, 0}));
      CHECK((f.at(29, 0) == ledwall::Rgb{0, 255, 0}));
      CHECK((f.at(0, 9) == ledwall::Rgb{0, 0, 255}));
      CHECK((f.at(29, 9) == ledwall::Rgb{255, 255, 255}));
      CHECK((f.at(14, 4) == ledwall::Rgb{255, 255, 0}));
    }
  }
}

void test_adversarial_expression_unknown_restores_nothing() {
  // An unknown expression renders blank; a subsequent known one renders
  // normally from the same buffer (no hidden state between calls).
  ledwall::FrameBuffer f;
  ledwall::renderer::RenderInput in;
  in.kind = ledwall::ModeKind::Expression;
  in.name = "mysterious";
  in.now_ms = 99;
  ledwall::renderer::render(f, in);
  CHECK_EQ_INT(count_lit(f), 0);
  in.name = "alert";
  ledwall::renderer::render(f, in);
  CHECK_EQ_INT(count_lit(f), 76);
}
