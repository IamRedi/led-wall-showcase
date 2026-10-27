#include "api/registry.h"
#include "effects/combined_renderers.h"
#include "test_framework.h"

namespace {
bool lit(ledwall::Rgb p) { return p != ledwall::kRgbBlack; }
}  // namespace

void test_v41_all_ten_effects_have_coordinated_real_frames() {
  const char* names[10] = {"fireworks", "meteor_impact", "energy_bloom", "rainbow_river",
                           "heartbeat", "storm", "lava_fountain", "rain_drain",
                           "aurora_current", "starstream"};
  const uint32_t times[10] = {2750, 1300, 1400, 100, 100, 1850, 1700, 1300, 100, 1450};
  for (uint8_t n = 0; n < 10; ++n) {
    ledwall::FrameBuffer a, b;
    ledwall::TailFramebuffer ta, tb;
    ta.configure(150); tb.configure(150);
    CHECK(ledwall::valid_animation(names[n]));
    CHECK(ledwall::effects::combined_animation_name(names[n]));
    CHECK(ledwall::effects::render_combined(names[n], a, ta, times[n]));
    CHECK(ledwall::effects::render_combined(names[n], b, tb, times[n]));
    bool matrix_lit = false, tail_lit = false;
    for (uint16_t i = 0; i < ledwall::kLedCount; ++i) {
      CHECK(a.data()[i] == b.data()[i]);
      matrix_lit = matrix_lit || lit(a.data()[i]);
    }
    for (uint16_t i = 0; i < 150; ++i) {
      CHECK(ta.at(i) == tb.at(i));
      tail_lit = tail_lit || lit(ta.at(i));
    }
    CHECK(matrix_lit);
    CHECK(tail_lit == (n != 0));  // Fireworks tail goes dark after Matrix burst
  }
  CHECK(!ledwall::effects::combined_animation_name("scanner"));
}

void test_v41_rainbow_anchor_color_continuity() {
  ledwall::FrameBuffer matrix;
  ledwall::TailFramebuffer tail;
  tail.configure(150);
  CHECK(ledwall::effects::render_combined("rainbow_river", matrix, tail, 350));
  CHECK(matrix.at(14, 9) == tail.at(0));
  CHECK(matrix.at(15, 9) == tail.at(0));
  CHECK(!ledwall::effects::render_combined("bogus", matrix, tail, 350));
}
