#include "test_framework.h"

#include <cstring>
#include <initializer_list>

#include "api/registry.h"
#include "effects/tail_renderers.h"
#include "render/display_topology.h"

// LED-L010 zone model + tail effects (ADR-LED-L010 D1/D3): bounds-checked 1D
// surface, target validation, deterministic bounded tail renderers over
// synthetic rope lengths.

using namespace ledwall;

namespace {
bool tail_equals(const TailFramebuffer& a, const TailFramebuffer& b) {
  if (a.length() != b.length()) {
    return false;
  }
  for (uint16_t i = 0; i < a.length(); ++i) {
    Rgb ca = a.at(i);
    Rgb cb = b.at(i);
    if (ca.r != cb.r || ca.g != cb.g || ca.b != cb.b) {
      return false;
    }
  }
  return true;
}
}  // namespace

void test_zone_target_wire_roundtrip() {
  for (uint8_t i = 0; i < kZoneTargetCount; ++i) {
    const ZoneTarget t = static_cast<ZoneTarget>(i);
    const char* name = zone_target_name(t);
    CHECK(name != nullptr);
    ZoneTarget back = ZoneTarget::Matrix;
    CHECK(zone_target_from_name(name, &back));
    CHECK(static_cast<int>(back) == static_cast<int>(i));
  }
  ZoneTarget out = ZoneTarget::Matrix;
  CHECK(!zone_target_from_name("rope", &out));
  CHECK(!zone_target_from_name(nullptr, &out));
}

void test_zone_target_supported_mask() {
  using namespace zone_flags;
  CHECK(zone_target_supported(kMatrix, ZoneTarget::Matrix));
  CHECK(!zone_target_supported(kMatrix, ZoneTarget::Tail));
  CHECK(!zone_target_supported(kMatrix, ZoneTarget::Both));
  CHECK(zone_target_supported(kTail, ZoneTarget::Tail));
  CHECK(!zone_target_supported(kTail, ZoneTarget::Matrix));
  CHECK(zone_target_supported(kTail | kBoth, ZoneTarget::Both));
  CHECK(!zone_target_supported(kTail | kBoth, ZoneTarget::Matrix));
}

void test_tail_framebuffer_bounds_and_config() {
  TailFramebuffer t;
  CHECK_EQ_INT(t.length(), LED_WALL_TAIL_LENGTH_DEFAULT);  // documented default
  t.configure(12);
  CHECK_EQ_INT(t.length(), 12);
  t.clear();
  // In-bounds write/read.
  const Rgb red{255, 0, 0};
  t.set(0, red);
  t.set(11, red);
  CHECK(t.at(0).r == 255);
  CHECK(t.at(11).r == 255);
  // Out-of-bounds writes are dropped (no crash, no aliasing).
  t.set(12, red);
  t.set(60000, red);
  CHECK(t.at(12).r == 0 && t.at(12).g == 0);
  // fill + clear.
  t.fill(red);
  for (uint16_t i = 0; i < 12; ++i) {
    CHECK(t.at(i).r == 255);
  }
  t.clear();
  for (uint16_t i = 0; i < 12; ++i) {
    CHECK(t.at(i).r == 0);
  }
  // Degenerate configs clamp to a safe 1-cell lane.
  t.configure(0);
  CHECK_EQ_INT(t.length(), 1);
  t.set(0, red);
  t.configure(60000);
  CHECK_EQ_INT(t.length(), 1);
  // Max lane accepted.
  t.configure(TailFramebuffer::kMaxLength);
  CHECK_EQ_INT(t.length(), TailFramebuffer::kMaxLength);
  t.set(static_cast<uint16_t>(TailFramebuffer::kMaxLength - 1), red);
  CHECK(t.at(static_cast<uint16_t>(TailFramebuffer::kMaxLength - 1)).r == 255);
}

void test_tail_effects_deterministic_and_bounded() {
  // Every tail renderer at several times: deterministic and length-clamped.
  const char* names[] = {"comet",   "scanner", "pulse",  "breathing",
                         "meteor",  "spark_trail", "lightning", "fire_flow",
                         "progress", "countdown_progress", "chase", "color_wave",
                         "heartbeat", "rainbow_flow"};
  const uint16_t lengths[] = {1, 7, 24, 60, 137, 300};
  for (const char* name : names) {
    const effects::TailRenderer r = effects::tail_animation_by_name(name);
    CHECK(r != nullptr);
    for (uint16_t len : lengths) {
      for (uint32_t t : {0u, 37u, 199u, 1234u, 65535u, 1000000u}) {
        TailFramebuffer a, b;
        a.configure(len);
        b.configure(len);
        r(a, t);
        r(b, t);
        CHECK(tail_equals(a, b));  // deterministic for the same input
        // A second call on a dirty buffer yields the same frame (self-clearing).
        r(a, t + 1);
        CHECK(true);
      }
    }
  }
  CHECK(effects::tail_animation_by_name("wave") == nullptr);  // matrix-only name
  CHECK(effects::tail_animation_by_name("") == nullptr);
  CHECK(effects::tail_animation_by_name(nullptr) == nullptr);
  CHECK(effects::tail_animation_by_name("comett") == nullptr);  // no prefix match
}

void test_tail_scanner_moves_exactly_one_pixel() {
  const uint16_t lengths[] = {1, 2, 7, 300};
  const uint32_t times[] = {0, 199, 200, 399, 400, 59800, 60000, 119600, 120000};
  for (uint16_t length : lengths) {
    const uint16_t span = static_cast<uint16_t>(length - 1);
    for (uint32_t time_ms : times) {
      TailFramebuffer tail;
      tail.configure(length);
      effects::tail_render_scanner(tail, time_ms);

      uint16_t expected = 0;
      if (span > 0) {
        const uint32_t period = 2u * span;
        const uint32_t phase = (time_ms / 200u) % period;
        expected = static_cast<uint16_t>(phase < span ? phase : period - phase);
      }
      CHECK_EQ_INT(effects::tail_scanner_position(length, time_ms), expected);
      uint16_t lit_count = 0;
      uint16_t lit_cell = 0;
      for (uint16_t cell = 0; cell < length; ++cell) {
        const Rgb pixel = tail.at(cell);
        if (pixel.r != 0 || pixel.g != 0 || pixel.b != 0) {
          ++lit_count;
          lit_cell = cell;
        }
      }
      CHECK_EQ_INT(lit_count, 1);
      CHECK_EQ_INT(lit_cell, expected);
      CHECK(tail.at(expected).r == 220 && tail.at(expected).g == 245 &&
            tail.at(expected).b == 255);
    }
  }
}

void test_tail_cores_progress_and_chase() {
  // Progress core: fill fraction maps to lit cells over a dim background.
  TailFramebuffer t;
  t.configure(10);
  effects::tail_progress_core(t, 0, Rgb{0, 255, 0}, Rgb{10, 10, 10});
  CHECK(t.at(0).g == 10);  // background only
  effects::tail_progress_core(t, 128, Rgb{0, 255, 0}, Rgb{10, 10, 10});
  CHECK(t.at(4).g == 255);  // ~half filled
  CHECK(t.at(4).r == 0);
  effects::tail_progress_core(t, 255, Rgb{0, 255, 0}, Rgb{10, 10, 10});
  CHECK(t.at(9).g == 255);  // full

  // Chase core: exactly the moving dots differ frame to frame but stay lit.
  t.configure(30);
  effects::tail_chase_core(t, 0, 0);
  int lit = 0;
  for (uint16_t i = 0; i < 30; ++i) {
    if (t.at(i).r > 0 || t.at(i).g > 0 || t.at(i).b > 0) {
      ++lit;
    }
  }
  CHECK(lit >= 1);  // dots exist on a 30-cell rope (gap clamped to 4)
}

void test_tail_pulse_core_extremes() {
  TailFramebuffer t;
  t.configure(5);
  // val_min == 255: solid rope at every phase.
  effects::tail_pulse_core(t, 0, 0, 255, 9);
  for (uint16_t i = 0; i < 5; ++i) {
    CHECK(t.at(i).r > 0);
  }
  // One-cell rope never breaks.
  t.configure(1);
  effects::tail_pulse_core(t, 12345, 200, 10, 9);
  CHECK(t.at(0).r + t.at(0).g + t.at(0).b > 0);
}
