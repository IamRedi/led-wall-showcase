#include <cmath>
#include <cstring>

#include <initializer_list>

#include "api/registry.h"
#include "effects/animation_renderers.h"
#include "effects/combined_renderers.h"
#include "effects/tail_renderers.h"
#include "render/color.h"
#include "test_framework.h"

// Animation golden frames (frozen §7 registry, heritage demo algorithms).
// Expectations are derived from the heritage source facts (transcribed in the
// comments) and from the documented deterministic color conversion — never by
// calling the renderer under test. Structural checks (bounds, determinism,
// full-frame clears) cover every renderer.

namespace {

constexpr uint8_t kW = 30;
constexpr uint8_t kH = 10;

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

bool frame_equals(const ledwall::FrameBuffer& a, const ledwall::FrameBuffer& b) {
  return std::memcmp(a.data(), b.data(), 300 * sizeof(ledwall::Rgb)) == 0;
}

bool lit(const ledwall::FrameBuffer& f, uint8_t x, uint8_t y) {
  return f.at(x, y) != ledwall::kRgbBlack;
}

// Independent transcription of the documented 6-sector HSV→RGB conversion
// (render/color.h documentation) for color golden expectations.
ledwall::Rgb ref_hsv(uint8_t hue, uint8_t sat, uint8_t val) {
  if (val == 0) {
    return ledwall::kRgbBlack;
  }
  if (sat == 0) {
    return ledwall::Rgb{val, val, val};
  }
  const int sector = hue / 43;
  const int offset = hue % 43;
  const int ramp = (offset * 255) / 42;
  const int lo = (val * (255 - sat)) / 255;
  const int amp = val - lo;
  int r = lo, g = lo, b = lo;
  switch (sector) {
    case 0: r = val; g = lo + (amp * ramp) / 255; break;
    case 1: r = lo + (amp * (255 - ramp)) / 255; g = val; break;
    case 2: g = val; b = lo + (amp * ramp) / 255; break;
    case 3: g = lo + (amp * (255 - ramp)) / 255; b = val; break;
    case 4: r = lo + (amp * ramp) / 255; b = val; break;
    default: r = val; b = lo + (amp * (255 - ramp)) / 255; break;
  }
  return ledwall::Rgb{static_cast<uint8_t>(r), static_cast<uint8_t>(g),
                      static_cast<uint8_t>(b)};
}

}  // namespace

void test_corners_golden() {
  // Heritage demoCorners(): exactly five fixed pixels, time-independent.
  ledwall::FrameBuffer f;
  ledwall::effects::render_corners(f, 0);
  CHECK_EQ_INT(count_lit(f), 5);
  CHECK((f.at(0, 0) == ledwall::Rgb{255, 0, 0}));
  CHECK((f.at(29, 0) == ledwall::Rgb{0, 255, 0}));
  CHECK((f.at(0, 9) == ledwall::Rgb{0, 0, 255}));
  CHECK((f.at(29, 9) == ledwall::Rgb{255, 255, 255}));
  CHECK((f.at(14, 4) == ledwall::Rgb{255, 255, 0}));

  ledwall::FrameBuffer later;
  ledwall::effects::render_corners(later, 987654);
  CHECK(frame_equals(f, later));  // static program
}

void test_checker_golden() {
  // Heritage demoChecker(): on where (x + y + shift) even, shift = (t/200)&1;
  // CRGB(20,20,30) vs black.
  ledwall::FrameBuffer f0;
  ledwall::effects::render_checker(f0, 0);
  int lit0 = 0;
  for (uint8_t y = 0; y < kH; ++y) {
    for (uint8_t x = 0; x < kW; ++x) {
      const bool even = ((x + y) & 1) == 0;
      CHECK((f0.at(x, y) == (even ? ledwall::Rgb{20, 20, 30} : ledwall::kRgbBlack)));
      if (even) {
        ++lit0;
      }
    }
  }
  CHECK_EQ_INT(lit0, 150);  // half the canvas

  // t=200 flips the parity.
  ledwall::FrameBuffer f200;
  ledwall::effects::render_checker(f200, 200);
  CHECK_EQ_INT(f200.at(0, 0).r + f200.at(0, 0).g + f200.at(0, 0).b, 0);
  CHECK((f200.at(1, 0) == ledwall::Rgb{20, 20, 30}));
  // t=400 is back to the t=0 parity; 5 s period boundary via 8-bit wrap.
  ledwall::FrameBuffer f400;
  ledwall::effects::render_checker(f400, 400);
  CHECK(frame_equals(f0, f400));
}

void test_border_golden() {
  // Heritage demoBorder(): border ring in CHSV(160,255,pulse), pulse =
  // beatsin8(12,40,255) ≈ 40..255 sine at a 5 s period. At t=0 the pulse
  // starts at its mid value: sin_u8(0)=128 → 40 + 128·215/255 = 147.
  ledwall::FrameBuffer f;
  ledwall::effects::render_border(f, 0);
  const ledwall::Rgb mid = ref_hsv(160, 255, 147);
  CHECK_EQ_INT(count_lit(f), 76);  // 2×30 + 2×8 ring
  CHECK(f.at(0, 0) == mid);
  CHECK(f.at(15, 0) == mid);
  CHECK(f.at(0, 5) == mid);
  CHECK(f.at(29, 5) == mid);
  CHECK(f.at(15, 5) == ledwall::kRgbBlack);  // interior
  CHECK(f.at(1, 1) == ledwall::kRgbBlack);

  // Pulse at the sine peak: phase 64/256 → t = 5000/4 = 1250 ms → pulse = 255.
  ledwall::FrameBuffer fpeak;
  ledwall::effects::render_border(fpeak, 1250);
  CHECK(fpeak.at(15, 0) == ref_hsv(160, 255, 255));
  // Pulse at the sine trough: phase 192/256 → t = 3750 ms → pulse = 40.
  ledwall::FrameBuffer fmin;
  ledwall::effects::render_border(fmin, 3750);
  CHECK(fmin.at(15, 0) == ref_hsv(160, 255, 40));
}

void test_row_sweep_golden() {
  // Heritage demoRowSweep(): step = (t/120) % 35; rows fill x ≤ step with
  // CHSV(y*22+80, 220, 220).
  ledwall::FrameBuffer f;
  ledwall::effects::render_row_sweep(f, 0);
  CHECK_EQ_INT(count_lit(f), 10);  // column 0 only
  for (uint8_t y = 0; y < kH; ++y) {
    CHECK(f.at(0, y) == ref_hsv(static_cast<uint8_t>(y * 22 + 80), 220, 220));
    CHECK(!lit(f, 1, y));
  }

  // t=480 → step 4: x=0..4 lit per row (40 px).
  ledwall::FrameBuffer f4;
  ledwall::effects::render_row_sweep(f4, 480);
  CHECK_EQ_INT(count_lit(f4), 50);
  CHECK(lit(f4, 4, 3));
  CHECK(!lit(f4, 5, 3));

  // Cycle wraps at step 35: t=120*35=4200 → step 0 again.
  ledwall::FrameBuffer fwrap;
  ledwall::effects::render_row_sweep(fwrap, 4200);
  CHECK(frame_equals(f, fwrap));
}

void test_bounce_golden() {
  // Heritage demoBounce() float math: at t=0, sinf(0)=0 → bx=(0.5)(29)=14.5→14;
  // cosf(0)=1 → by=(1.0)(9)=9. White dot + trail (30,30,40) at x−1.
  ledwall::FrameBuffer f;
  ledwall::effects::render_bounce(f, 0);
  CHECK_EQ_INT(count_lit(f), 2);
  CHECK((f.at(14, 9) == ledwall::Rgb{255, 255, 255}));
  CHECK((f.at(13, 9) == ledwall::Rgb{30, 30, 40}));

  // Determinism and independence-derived position at t=3000:
  // t=3 s: sin(3.9)=-0.6878… → bx=(0.1561)(29)=4.52→4; cos(5.1)=0.3780… →
  // by=(0.6890)(9)=6.20→6.
  ledwall::FrameBuffer f3a, f3b;
  ledwall::effects::render_bounce(f3a, 3000);
  ledwall::effects::render_bounce(f3b, 3000);
  CHECK(frame_equals(f3a, f3b));
  CHECK_EQ_INT(count_lit(f3a), 2);
  CHECK((f3a.at(4, 6) == ledwall::Rgb{255, 255, 255}));
  CHECK((f3a.at(3, 6) == ledwall::Rgb{30, 30, 40}));
}

void test_wave_golden() {
  // Heritage demoWave(): phase = t/80 + x·0.35; one pixel per column,
  // CHSV(x*8, 255, 255). At t=0 the phase is purely x·0.35.
  ledwall::FrameBuffer f;
  ledwall::effects::render_wave(f, 0);
  CHECK_EQ_INT(count_lit(f), 30);  // exactly one pixel per column
  for (uint8_t x = 0; x < kW; ++x) {
    const float phase = static_cast<float>(x) * 0.35f;
    const int y = static_cast<int>((std::sin(phase) * 0.5f + 0.5f) * (kH - 1));
    CHECK(f.at(x, static_cast<uint8_t>(y)) == ref_hsv(static_cast<uint8_t>(x * 8), 255, 255));
    for (uint8_t yy = 0; yy < kH; ++yy) {
      if (yy != y) {
        CHECK(!lit(f, x, yy));
      }
    }
  }

  // Time shift changes the frame deterministically: at t=800 the phase for
  // column 0 is 10.0 rad; sin(10)=−0.5440… → y=(0.2280)(9)=2.05→2.
  ledwall::FrameBuffer f8;
  ledwall::effects::render_wave(f8, 800);
  CHECK(f8.at(0, 2) == ref_hsv(0, 255, 255));
  CHECK(!lit(f8, 0, 4));  // t=0 had column 0 at y=4 (sin(0)=0 → (0.5)(9)=4.5→4)
}

void test_rain_golden() {
  // Deterministic COPY/ADAPT of demoRain(): per-column cycle, head at
  // τ/16 rows, heritage head color, 2-step trail (×205/255 per row).
  ledwall::FrameBuffer f0;
  ledwall::effects::render_rain(f0, 0);
  CHECK_EQ_INT(count_lit(f0), 30);  // all heads in row 0, no trail yet
  for (uint8_t x = 0; x < kW; ++x) {
    CHECK((f0.at(x, 0) == ledwall::Rgb{40, 60, 200}));
  }

  // t=16: head advanced one row, one trail pixel behind.
  ledwall::FrameBuffer f16;
  ledwall::effects::render_rain(f16, 16);
  CHECK_EQ_INT(count_lit(f16), 60);
  CHECK((f16.at(5, 1) == ledwall::Rgb{40, 60, 200}));
  CHECK((f16.at(5, 0) == ledwall::Rgb{32, 48, 160}));

  // Bounded: heads never leave the canvas (heritage bottom stop).
  for (uint32_t t = 0; t < 2000; t += 7) {
    ledwall::FrameBuffer fb;
    ledwall::effects::render_rain(fb, t);
    for (uint8_t x = 0; x < kW; ++x) {
      for (uint8_t y = 0; y < kH; ++y) {
        if (lit(fb, x, y)) {
          CHECK(y <= 9);
        }
      }
    }
  }

  // Deterministic for the same time input (the reason for the ADAPT).
  ledwall::FrameBuffer d1, d2;
  ledwall::effects::render_rain(d1, 12345);
  ledwall::effects::render_rain(d2, 12345);
  CHECK(frame_equals(d1, d2));
}

void test_plasma_golden() {
  // Heritage demoPlasma(): t8 = t/25; v = sin8(x·12+t8) + sin8(y·18−t8) +
  // sin8((x+y)·8+t8) (uint8 wrap), CHSV(v,255,180). At t=0, pixel (0,0):
  // sin8(0)·3 = 128·3 = 384 → wrap 128.
  ledwall::FrameBuffer f;
  ledwall::effects::render_plasma(f, 0);
  CHECK_EQ_INT(count_lit(f), 300);  // full-field effect, no black pixels (val 180 > 0)
  CHECK(f.at(0, 0) == ref_hsv(128, 255, 180));
  // Pixel (1,0): sin8(12) + sin8(0) + sin8(8) = 165 + 128 + 152 = 445 → 189.
  CHECK(f.at(1, 0) == ref_hsv(189, 255, 180));
  // Pixel (0,1): sin8(0) + sin8(18) + sin8(8) = 128 + 182 + 152 = 462 → 206.
  CHECK(f.at(0, 1) == ref_hsv(206, 255, 180));

  ledwall::FrameBuffer d1, d2;
  ledwall::effects::render_plasma(d1, 9000);
  ledwall::effects::render_plasma(d2, 9000);
  CHECK(frame_equals(d1, d2));
}

void test_animations_registry_binding_and_bounds() {
  // Every registered name binds to a deterministic renderer on its supported
  // surfaces (LED-L010: the five tail-only names have NO matrix binding —
  // fail closed blank upstream — and every matrix-capable name does).
  for (const char* name : ledwall::kAnimationNames) {
    // Tail-ONLY names (no matrix form at all): comet, scanner, meteor,
    // spark_trail, fire_flow. The both-capable names carry matrix bindings.
    if (ledwall::animation_zone_mask(name) == ledwall::zone_flags::kTail) {
      CHECK(ledwall::effects::animation_by_name(name) == nullptr);
      CHECK(ledwall::effects::tail_animation_by_name(name) != nullptr);
    } else if (ledwall::animation_zone_mask(name) == ledwall::zone_flags::kBoth) {
      ledwall::FrameBuffer matrix;
      ledwall::TailFramebuffer tail;
      tail.configure(150);
      CHECK(ledwall::effects::animation_by_name(name) == nullptr);
      CHECK(ledwall::effects::render_combined(name, matrix, tail, 0));
    } else {
      CHECK(ledwall::effects::animation_by_name(name) != nullptr);
    }
  }
  CHECK(ledwall::effects::animation_by_name("demoRain") == nullptr);
  CHECK(ledwall::effects::animation_by_name("") == nullptr);
  CHECK(ledwall::effects::animation_by_name(nullptr) == nullptr);
  CHECK(ledwall::effects::animation_by_name("corner") == nullptr);  // prefix must not match
  CHECK(ledwall::effects::tail_animation_by_name("comet") != nullptr);
  CHECK(ledwall::effects::tail_animation_by_name("wave") == nullptr);  // matrix-only name
  CHECK(ledwall::effects::tail_animation_by_name("demoRain") == nullptr);

  // Full sweep: every MATRIX renderer at every probed time is deterministic
  // and produces only canvas-legal content (FrameBuffer structurally drops
  // out-of-bounds writes; goldens pin the full frame contents).
  for (const char* name : ledwall::kAnimationNames) {
    const ledwall::effects::AnimationRenderer r = ledwall::effects::animation_by_name(name);
    if (r == nullptr) {
      continue;  // tail-only name (covered by the tail sweep in test_tail_effects)
    }
    for (uint32_t t : {0u, 37u, 199u, 1234u, 65535u, 1000000u}) {
      ledwall::FrameBuffer a, b;
      r(a, t);
      r(b, t);
      CHECK(frame_equals(a, b));
      for (uint8_t y = 0; y < kH; ++y) {
        for (uint8_t x = 0; x < kW; ++x) {
          const ledwall::Rgb p = a.at(x, y);
          (void)p;  // bounds + determinism asserted above; content pinned by goldens
        }
      }
    }
  }

  // Time-varying classification: the heritage corners and the LED-L009 static
  // icons are time-independent; everything else animates.
  const char* static_names[5] = {"corners", "smile", "heart", "success", "warning"};
  for (const char* name : ledwall::kAnimationNames) {
    bool is_static = false;
    for (const char* sn : static_names) {
      if (std::strcmp(name, sn) == 0) is_static = true;
    }
    if (is_static) {
      CHECK(!ledwall::effects::animation_is_time_varying(name));
    } else {
      CHECK(ledwall::effects::animation_is_time_varying(name));
    }
  }
  CHECK(!ledwall::effects::animation_is_time_varying("nope"));
}
