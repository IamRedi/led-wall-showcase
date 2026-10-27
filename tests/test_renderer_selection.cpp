#include <cstring>

#include "api/command.h"
#include "effects/animation_renderers.h"
#include "render/color.h"
#include "render/renderer.h"
#include "test_framework.h"

// Renderer selection and fail-closed behavior (frozen §4→§7/§8/§11/§12): the
// renderer deterministically renders the CURRENT semantic program; unknown or
// unrenderable state renders the safe blank frame.

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

ledwall::FrameBuffer render_kind(ledwall::ModeKind kind, const char* name, uint32_t now,
                                 uint32_t started = 0) {
  ledwall::renderer::RenderInput in;
  in.kind = kind;
  in.name = name;
  in.started_ms = started;
  in.now_ms = now;
  ledwall::FrameBuffer f;
  ledwall::renderer::render(f, in);
  return f;
}

}  // namespace

void test_renderer_idle_is_blank() {
  const ledwall::FrameBuffer f = render_kind(ledwall::ModeKind::Idle, "", 123456);
  CHECK_EQ_INT(count_lit(f), 0);
}

void test_renderer_scene_and_animation_identical() {
  // §7: animation-class names are usable as scenes — the same name renders
  // the identical frame under both kinds.
  for (const char* name : ledwall::kAnimationNames) {
    const ledwall::FrameBuffer as_scene =
        render_kind(ledwall::ModeKind::Scene, name, 7777, 1000);
    const ledwall::FrameBuffer as_animation =
        render_kind(ledwall::ModeKind::Animation, name, 7777, 1000);
    CHECK(frame_equals(as_scene, as_animation));
  }
}

void test_renderer_unknown_names_fail_closed() {
  // Unknown registry names (impossible via the accepted API, defensive) and
  // empty names render blank, never garbage.
  CHECK_EQ_INT(count_lit(render_kind(ledwall::ModeKind::Scene, "demoRain", 5)), 0);
  CHECK_EQ_INT(count_lit(render_kind(ledwall::ModeKind::Animation, "corner", 5)), 0);
  CHECK_EQ_INT(count_lit(render_kind(ledwall::ModeKind::Scene, "", 5)), 0);
  CHECK_EQ_INT(count_lit(render_kind(ledwall::ModeKind::Expression, "curious", 5)), 0);
  CHECK_EQ_INT(count_lit(render_kind(ledwall::ModeKind::Expression, "", 5)), 0);
}

void test_renderer_conditional_kinds_fail_closed() {
  // Spectrum (mic unconfirmed, ADR-012) and paint (diagnostic) have no
  // renderers in this milestone: safe blank.
  CHECK_EQ_INT(count_lit(render_kind(ledwall::ModeKind::Spectrum, "spectrum.rows", 5)), 0);
  CHECK_EQ_INT(count_lit(render_kind(ledwall::ModeKind::Paint, "", 5)), 0);
}

void test_renderer_text_dispatch() {
  ledwall::TextProgram t;
  std::strncpy(t.current, "HI", sizeof(t.current) - 1);
  t.started_ms = 1000;
  t.scrolling = false;

  ledwall::renderer::RenderInput in;
  in.kind = ledwall::ModeKind::Text;
  in.name = "";
  in.text = &t;
  in.started_ms = 1000;
  in.now_ms = 1000;
  ledwall::FrameBuffer f;
  ledwall::renderer::render(f, in);
  // Static "HI" centered: x0 = 9 (width 12); H col0 full at x=9.
  CHECK(f.at(9, 2) != ledwall::kRgbBlack);
  CHECK_EQ_INT(count_lit(f), 28);

  // Malformed: text mode without a program → blank.
  ledwall::renderer::RenderInput broken;
  broken.kind = ledwall::ModeKind::Text;
  broken.name = "";
  broken.text = nullptr;
  broken.now_ms = 1000;
  ledwall::FrameBuffer fb;
  ledwall::renderer::render(fb, broken);
  CHECK_EQ_INT(count_lit(fb), 0);
}

void test_renderer_expression_dispatch_and_restore() {
  // alert renders the red pulsing border (76-px ring); at now=100/started=0
  // the elapsed is 100 ms → phase (100·256)/1250 = 20 → sin 188 → pulse 198.
  const ledwall::FrameBuffer alert = render_kind(ledwall::ModeKind::Expression, "alert", 100, 0);
  CHECK_EQ_INT(count_lit(alert), 76);
  CHECK(alert.at(15, 0) == ledwall::hsv2rgb(0, 255, 198));

  // idle/sleeping render the safe rest frame (blank).
  CHECK_EQ_INT(count_lit(render_kind(ledwall::ModeKind::Expression, "idle", 5)), 0);
  CHECK_EQ_INT(count_lit(render_kind(ledwall::ModeKind::Expression, "sleeping", 5)), 0);

  // thinking renders the slow wave (30 columns lit).
  CHECK_EQ_INT(count_lit(render_kind(ledwall::ModeKind::Expression, "thinking", 0, 0)), 30);

  // Determinism at the same state/time input.
  const ledwall::FrameBuffer h1 = render_kind(ledwall::ModeKind::Expression, "happy", 4242, 0);
  const ledwall::FrameBuffer h2 = render_kind(ledwall::ModeKind::Expression, "happy", 4242, 0);
  CHECK(frame_equals(h1, h2));
}

void test_renderer_time_varying_classification() {
  ledwall::TextProgram scroll;
  std::strncpy(scroll.current, "HI", sizeof(scroll.current) - 1);
  scroll.started_ms = 0;
  scroll.scrolling = true;
  ledwall::TextProgram stat;
  std::strncpy(stat.current, "HI", sizeof(stat.current) - 1);
  stat.started_ms = 0;
  stat.scrolling = false;

  auto varying = [&](ledwall::ModeKind kind, const char* name,
                     ledwall::TextProgram* t) {
    ledwall::renderer::RenderInput in;
    in.kind = kind;
    in.name = name;
    in.text = t;
    in.now_ms = 0;
    return ledwall::renderer::program_is_time_varying(in);
  };

  CHECK(!varying(ledwall::ModeKind::Idle, "", nullptr));
  CHECK(!varying(ledwall::ModeKind::Text, "", &stat));      // static text: no re-render
  CHECK(varying(ledwall::ModeKind::Text, "", &scroll));     // scrolling: re-render
  CHECK(!varying(ledwall::ModeKind::Scene, "corners", nullptr));  // static program
  CHECK(varying(ledwall::ModeKind::Scene, "wave", nullptr));
  CHECK(varying(ledwall::ModeKind::Animation, "rain", nullptr));
  CHECK(!varying(ledwall::ModeKind::Expression, "idle", nullptr));
  CHECK(!varying(ledwall::ModeKind::Expression, "error", nullptr));  // static border
  CHECK(varying(ledwall::ModeKind::Expression, "alert", nullptr));   // pulse
  CHECK(!varying(ledwall::ModeKind::Spectrum, "", nullptr));  // fail-closed blank is static
  CHECK(!varying(ledwall::ModeKind::Scene, "nope", nullptr)); // unknown → static blank
}

void test_text_program_completion_semantics() {
  // Static text: complete exactly when its first frame has been shown.
  ledwall::TextProgram stat;
  std::strncpy(stat.current, "HI", sizeof(stat.current) - 1);
  stat.started_ms = 1000;
  stat.scrolling = false;
  CHECK(!ledwall::renderer::text_program_complete(stat, 1000, false));
  CHECK(!ledwall::renderer::text_program_complete(stat, 1100, false));
  CHECK(ledwall::renderer::text_program_complete(stat, 1100, true));

  // Scrolling text: complete after one full traversal (speed × span).
  ledwall::TextProgram scroll;
  std::strncpy(scroll.current, "HI", sizeof(scroll.current) - 1);
  scroll.started_ms = 0;
  scroll.scrolling = true;
  scroll.speed_ms_per_px = 40;
  // span("HI") = 42 → cycle = 1680 ms.
  CHECK(!ledwall::renderer::text_program_complete(scroll, 1679, true));
  CHECK(ledwall::renderer::text_program_complete(scroll, 1680, false));  // even without a show
  CHECK(ledwall::renderer::text_program_complete(scroll, 500000, false));

  // Empty program never completes.
  ledwall::TextProgram none;
  CHECK(!ledwall::renderer::text_program_complete(none, 99999, true));

  // uint32 rollover: elapsed = now − started wraps safely.
  ledwall::TextProgram rolled;
  std::strncpy(rolled.current, "HI", sizeof(rolled.current) - 1);
  rolled.started_ms = 0xFFFFFF00u;
  rolled.scrolling = true;
  rolled.speed_ms_per_px = 40;
  CHECK(!ledwall::renderer::text_program_complete(rolled, 100, false));   // elapsed 356
  CHECK(ledwall::renderer::text_program_complete(rolled, 1680, false));   // elapsed 1680
}
