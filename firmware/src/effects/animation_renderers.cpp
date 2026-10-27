#include "effects/animation_renderers.h"
#include "effects/combined_renderers.h"

#include <cstring>
#include <math.h>

#include "effects/tail_renderers.h"
#include "render/color.h"

namespace ledwall {
namespace effects {

namespace {

constexpr uint8_t kW = kMatrixWidth;
constexpr uint8_t kH = kMatrixHeight;

}  // namespace

// Heritage demoCorners() (COPY): four fixed corner pixels + yellow center,
// time-independent.
void render_corners(FrameBuffer& frame, uint32_t t_ms) {
  (void)t_ms;
  frame.clear();
  frame.set(0, 0, Rgb{255, 0, 0});                // CRGB::Red
  frame.set(kW - 1, 0, Rgb{0, 255, 0});           // CRGB::Green
  frame.set(0, kH - 1, Rgb{0, 0, 255});           // CRGB::Blue
  frame.set(kW - 1, kH - 1, Rgb{255, 255, 255});  // CRGB::White
  frame.set(14, 4, Rgb{255, 255, 0});             // CRGB::Yellow center
}

// Heritage demoChecker() (COPY): 200 ms parity shift, dim blue-ish squares.
void render_checker(FrameBuffer& frame, uint32_t t_ms) {
  frame.clear();
  const uint8_t shift = static_cast<uint8_t>((t_ms / 200) & 1);
  for (uint8_t y = 0; y < kH; ++y) {
    for (uint8_t x = 0; x < kW; ++x) {
      const bool on = ((x + y + shift) & 1) == 0;
      frame.set(static_cast<int16_t>(x), static_cast<int16_t>(y),
                on ? Rgb{20, 20, 30} : kRgbBlack);
    }
  }
}

// Heritage demoBorder() (COPY/ADAPT): pulsing hue-160 border; the heritage
// beatsin8(12,40,255) pulse runs through the deterministic color.h equivalent.
void render_border(FrameBuffer& frame, uint32_t t_ms) {
  frame.clear();
  const Rgb c = hsv2rgb(160, 255, border_pulse(t_ms));
  for (uint8_t x = 0; x < kW; ++x) {
    frame.set(static_cast<int16_t>(x), 0, c);
    frame.set(static_cast<int16_t>(x), kH - 1, c);
  }
  for (uint8_t y = 1; y < kH - 1; ++y) {
    frame.set(0, static_cast<int16_t>(y), c);
    frame.set(kW - 1, static_cast<int16_t>(y), c);
  }
}

// Heritage demoRowSweep() (COPY): each row fills left→right, 120 ms per
// column, 35-step cycle; row hue y*22+80.
void render_row_sweep(FrameBuffer& frame, uint32_t t_ms) {
  frame.clear();
  const uint8_t step = static_cast<uint8_t>((t_ms / 120) % (kW + 5));
  for (uint8_t y = 0; y < kH; ++y) {
    for (uint8_t x = 0; x < kW && x <= step; ++x) {
      frame.set(static_cast<int16_t>(x), static_cast<int16_t>(y),
                hsv2rgb(static_cast<uint8_t>(y * 22 + 80), 220, 220));
    }
  }
}

// Heritage demoBounce() (COPY): white dot on a Lissajous path (float heritage
// math preserved verbatim) with a dim trail pixel behind it.
void render_bounce(FrameBuffer& frame, uint32_t t_ms) {
  frame.clear();
  const float t = static_cast<float>(t_ms) / 1000.0f;
  const int16_t bx =
      static_cast<int16_t>((sinf(t * 1.3f) * 0.5f + 0.5f) * (kW - 1));
  const int16_t by =
      static_cast<int16_t>((cosf(t * 1.7f) * 0.5f + 0.5f) * (kH - 1));
  frame.set(bx, by, Rgb{255, 255, 255});  // CRGB::White
  // Heritage trail: one pixel to the left of the dot, clamped at x=0.
  frame.set(static_cast<int16_t>(bx > 0 ? bx - 1 : 0), by, Rgb{30, 30, 40});
}

// Heritage demoWave() (COPY): one lit pixel per column on a travelling sine;
// `time_divisor` is the heritage phase divisor (80 = demo default), hue
// sweeping with x from `hue_offset`.
void render_wave_frame(FrameBuffer& frame, uint32_t t_ms, uint32_t time_divisor,
                       uint8_t hue_offset) {
  frame.clear();
  for (uint8_t x = 0; x < kW; ++x) {
    const float phase = static_cast<float>(t_ms) / static_cast<float>(time_divisor) +
                        static_cast<float>(x) * 0.35f;
    const int16_t y =
        static_cast<int16_t>((sinf(phase) * 0.5f + 0.5f) * (kH - 1));
    frame.set(static_cast<int16_t>(x), y,
              hsv2rgb(static_cast<uint8_t>(x * 8 + hue_offset), 255, 255));
  }
}

// Heritage demoRain() (COPY/ADAPT): blue drops falling with fading trails.
// The heritage version drew random8() resets into static per-column state;
// the renderer contract requires determinism for the same time input, so the
// reset rhythm is a fixed index-derived per-column cycle (400..599 ms)
// instead of PRNG state. Drop speed (16 ms/row), head color, bottom-stop, and
// trail fade (heritage fadeToBlackBy 50 ≈ ×205/255 per row) are heritage
// values.
void render_rain(FrameBuffer& frame, uint32_t t_ms) {
  frame.clear();
  for (uint8_t x = 0; x < kW; ++x) {
    const uint8_t seed = static_cast<uint8_t>(x * 97 + 13);
    const uint32_t cycle_ms = 400u + (seed % 200u);
    const uint32_t tau = t_ms % cycle_ms;
    int16_t head = static_cast<int16_t>(tau / 16);
    if (head > kH - 1) {
      head = kH - 1;  // heritage drops stop at the bottom row
    }
    frame.set(static_cast<int16_t>(x), head, Rgb{40, 60, 200});
    if (head >= 1) {
      frame.set(static_cast<int16_t>(x), head - 1, Rgb{32, 48, 160});
    }
    if (head >= 2) {
      frame.set(static_cast<int16_t>(x), head - 2, Rgb{26, 39, 130});
    }
  }
}

// Heritage demoPlasma() (COPY): three additive 8-bit sines through a hue map;
// `time_divisor` is the heritage t divisor (25 = demo default).
void render_plasma_frame(FrameBuffer& frame, uint32_t t_ms, uint32_t time_divisor) {
  frame.clear();
  const uint8_t t = static_cast<uint8_t>(t_ms / time_divisor);
  for (uint8_t y = 0; y < kH; ++y) {
    for (uint8_t x = 0; x < kW; ++x) {
      const uint8_t v =
          static_cast<uint8_t>(sin_u8(static_cast<uint8_t>(x * 12 + t)) +
                               sin_u8(static_cast<uint8_t>(y * 18 - t)) +
                               sin_u8(static_cast<uint8_t>((x + y) * 8 + t)));
      frame.set(static_cast<int16_t>(x), static_cast<int16_t>(y), hsv2rgb(v, 255, 180));
    }
  }
}

void render_wave(FrameBuffer& frame, uint32_t t_ms) { render_wave_frame(frame, t_ms, 80, 0); }

void render_plasma(FrameBuffer& frame, uint32_t t_ms) {
  render_plasma_frame(frame, t_ms, 25);
}

// ------------------------------------------------------------------ LED-L009
// curated additions (reconstruction brief §15): deterministic icon/character
// and ambient visuals. Pure (frame, t_ms) functions, framebuffer-only writes,
// float math confined to documented formulas (same host/device libm contract
// as the heritage bounce/wave renderers).

namespace {

constexpr float kPi = 3.14159265f;

// Plot helpers (deterministic line/ring rasterization through the
// bounds-checked framebuffer).
void plot_line(FrameBuffer& frame, int16_t x0, int16_t y0, int16_t x1, int16_t y1, Rgb color) {
  int16_t dx = x1 - x0;
  int16_t dy = y1 - y0;
  int16_t sx = dx < 0 ? -1 : 1;
  int16_t sy = dy < 0 ? -1 : 1;
  dx = dx < 0 ? -dx : dx;
  dy = dy < 0 ? -dy : dy;
  int16_t err = dx - dy;
  int16_t x = x0;
  int16_t y = y0;
  while (true) {
    frame.set(x, y, color);
    if (x == x1 && y == y1) {
      break;
    }
    const int16_t e2 = static_cast<int16_t>(2 * err);
    if (e2 > -dy) {
      err -= dy;
      x += sx;
    }
    if (e2 < dx) {
      err += dx;
      y += sy;
    }
  }
}

// Face outline: normalized ellipse border (aspect-corrected for the 30×10
// pixel pitch). A pixel belongs to the face when its normalized radius is
// <= 1.15; the rim (0.80..1.15) renders bright, the interior dim so the
// dark eyes/mouth stay visible.
bool face_inside(int16_t x, int16_t y) {
  const float nx = static_cast<float>(x - 15) / 5.9f;
  const float ny = static_cast<float>(y - 4) / 4.4f;
  const float d = sqrtf(nx * nx + ny * ny);
  return d <= 1.15f;
}

Rgb face_dim{70, 55, 0};
Rgb heart_red{255, 40, 70};

void draw_face_body(FrameBuffer& frame) {
  for (uint8_t y = 0; y < kH; ++y) {
    for (uint8_t x = 0; x < kW; ++x) {
      if (face_inside(static_cast<int16_t>(x), static_cast<int16_t>(y))) {
        frame.set(static_cast<int16_t>(x), static_cast<int16_t>(y), face_dim);
      }
    }
  }
}

void draw_smile_eyes(FrameBuffer& frame, bool right_eye_open) {
  // Eyes: 1×2 vertical bars at the classic face positions; winking closes
  // the right eye to a horizontal dash.
  frame.set(12, 3, Rgb{0, 0, 0});
  frame.set(18, 3, Rgb{0, 0, 0});
  frame.set(12, 4, Rgb{0, 0, 0});
  if (right_eye_open) {
    frame.set(18, 4, Rgb{0, 0, 0});
  } else {
    frame.set(17, 4, Rgb{0, 0, 0});
    frame.set(19, 4, Rgb{0, 0, 0});
  }
}

// Deterministic heart predicate: classic implicit curve
// (x² + y² − 1)³ − x²·y³ ≤ 0 with aspect-corrected coordinates; `scale`
// shrinks/grows the heart for the pulse variant.
bool heart_pixel(int16_t x, int16_t y, float scale) {
  const float nx = static_cast<float>(x - 15) / (8.5f * scale);
  const float ny = static_cast<float>(4 - y) / (4.2f * scale) + 0.35f;
  const float a = nx * nx + ny * ny - 1.0f;
  return a * a * a - nx * nx * ny * ny * ny <= 0.0f;
}

}  // namespace

// Icon: smile — filled yellow face with dark eyes and a curved smile (static).
void render_smile(FrameBuffer& frame, uint32_t t_ms) {
  (void)t_ms;
  frame.clear();
  draw_face_body(frame);
  draw_smile_eyes(frame, /*right_eye_open=*/true);
  // Smile: downward arc x = 11..19.
  for (int16_t x = 11; x <= 19; ++x) {
    const float rel = static_cast<float>(x - 15) / 4.2f;
    const int16_t y = static_cast<int16_t>(4.6f + 1.8f * (1.0f - rel * rel));
    frame.set(x, y, Rgb{0, 0, 0});
  }
}

// Icon: wink — the smile face with the right eye closed on a blink cycle.
void render_wink(FrameBuffer& frame, uint32_t t_ms) {
  frame.clear();
  draw_face_body(frame);
  const bool eye_open = ((t_ms / 900) & 1u) == 0u;  // closed ~0.9 s of each 1.8 s
  draw_smile_eyes(frame, eye_open);
  for (int16_t x = 11; x <= 19; ++x) {
    const float rel = static_cast<float>(x - 15) / 4.2f;
    const int16_t y = static_cast<int16_t>(4.6f + 1.8f * (1.0f - rel * rel));
    frame.set(x, y, Rgb{0, 0, 0});
  }
}

// Icon: heart — red heart, static (formula-drawn).
void render_heart(FrameBuffer& frame, uint32_t t_ms) {
  (void)t_ms;
  frame.clear();
  for (uint8_t y = 0; y < kH; ++y) {
    for (uint8_t x = 0; x < kW; ++x) {
      if (heart_pixel(static_cast<int16_t>(x), static_cast<int16_t>(y), 1.0f)) {
        frame.set(static_cast<int16_t>(x), static_cast<int16_t>(y), heart_red);
      }
    }
  }
}

// Icon: heart_pulse — the heart breathing between two sizes.
void render_heart_pulse(FrameBuffer& frame, uint32_t t_ms) {
  frame.clear();
  const uint8_t phase = sin_u8(static_cast<uint8_t>((t_ms / 28) & 0xFF));  // ~1.4 s cycle
  const float scale = 0.80f + 0.28f * (static_cast<float>(phase) / 255.0f);
  for (uint8_t y = 0; y < kH; ++y) {
    for (uint8_t x = 0; x < kW; ++x) {
      if (heart_pixel(static_cast<int16_t>(x), static_cast<int16_t>(y), scale)) {
        frame.set(static_cast<int16_t>(x), static_cast<int16_t>(y), heart_red);
      }
    }
  }
}

// Icon: eyes — two eyes looking around (pupils orbit deterministically).
void render_eyes(FrameBuffer& frame, uint32_t t_ms) {
  frame.clear();
  const float t = static_cast<float>(t_ms) / 1000.0f;
  const int16_t pdx = static_cast<int16_t>(sinf(t * 0.9f) * 1.4f);   // -1..1
  const int16_t pdy = static_cast<int16_t>(cosf(t * 0.6f) * 1.4f);   // -1..1
  const Rgb eye_white{200, 200, 210};
  const Rgb pupil{40, 40, 60};
  for (uint8_t e = 0; e < 2; ++e) {
    const int16_t ex = (e == 0) ? 8 : 19;
    for (int16_t dy = -1; dy <= 1; ++dy) {
      for (int16_t dx = -1; dx <= 1; ++dx) {
        frame.set(static_cast<int16_t>(ex + dx), static_cast<int16_t>(4 + dy), eye_white);
      }
    }
    frame.set(static_cast<int16_t>(ex + pdx), static_cast<int16_t>(4 + pdy), pupil);
  }
}

// Icon: loading — a spinner dot orbiting a ring with a fading trail.
void render_loading(FrameBuffer& frame, uint32_t t_ms) {
  frame.clear();
  const float angle_step = kPi / 4.0f;  // 8 ring positions
  const float base = static_cast<float>(t_ms / 120 % 256) / 256.0f * 2.0f * kPi;
  for (uint8_t k = 0; k < 5; ++k) {  // head + 4 trail dots, decreasing value
    const float a = base - static_cast<float>(k) * angle_step * 0.7f;
    const int16_t x = static_cast<int16_t>(15.0f + 6.0f * cosf(a));
    const int16_t y = static_cast<int16_t>(5.0f + 3.4f * sinf(a));
    const uint8_t val = static_cast<uint8_t>(255u >> (k == 0 ? 0 : (k > 4 ? 4 : k)));
    frame.set(x, y, hsv2rgb(150, 60, val));
  }
}

// Icon: success — a green check mark (static).
void render_success(FrameBuffer& frame, uint32_t t_ms) {
  (void)t_ms;
  frame.clear();
  const Rgb green{40, 220, 70};
  plot_line(frame, 7, 5, 12, 9, green);
  plot_line(frame, 12, 9, 22, 1, green);
}

// Icon: warning — a triangle outline with an exclamation mark (static).
void render_warning(FrameBuffer& frame, uint32_t t_ms) {
  (void)t_ms;
  frame.clear();
  const Rgb amber{255, 170, 0};
  plot_line(frame, 15, 0, 5, 8, amber);
  plot_line(frame, 15, 0, 25, 8, amber);
  plot_line(frame, 5, 8, 25, 8, amber);
  frame.set(15, 3, amber);
  frame.set(15, 4, amber);
  frame.set(15, 6, amber);
}

// Icon: arrows — chevrons streaming to the right.
void render_arrows(FrameBuffer& frame, uint32_t t_ms) {
  frame.clear();
  const Rgb cyan{0, 200, 255};
  const int16_t phase = static_cast<int16_t>((t_ms / 120) % 9);
  for (int16_t k = -1; k < 4; ++k) {
    const int16_t x0 = static_cast<int16_t>(k * 9 + phase);
    plot_line(frame, x0, 2, static_cast<int16_t>(x0 + 3), 5, cyan);
    plot_line(frame, static_cast<int16_t>(x0 + 3), 5, x0, 8, cyan);
  }
}

// Ambient: aurora — vertical colour curtains drifting slowly.
void render_aurora(FrameBuffer& frame, uint32_t t_ms) {
  frame.clear();
  for (uint8_t x = 0; x < kW; ++x) {
    const uint8_t wave = sin_u8(static_cast<uint8_t>(x * 13 + t_ms / 25));
    const uint8_t height = static_cast<uint8_t>(3u + (static_cast<uint16_t>(wave) * 7u) / 255u);
    for (uint8_t y = 0; y < height; ++y) {
      const uint8_t fade = static_cast<uint8_t>(220u - y * 22u);
      frame.set(static_cast<int16_t>(x), static_cast<int16_t>(y),
                hsv2rgb(static_cast<uint8_t>(70 + x * 4 + t_ms / 40), 200, fade));
    }
  }
}

// Ambient: stars — sparse twinkling star field (per-pixel hash, no PRNG state).
void render_stars(FrameBuffer& frame, uint32_t t_ms) {
  frame.clear();
  for (uint8_t y = 0; y < kH; ++y) {
    for (uint8_t x = 0; x < kW; ++x) {
      const uint32_t hash = 2654435761u * (1u + x + 31u * y);
      if ((hash % 97u) < 7u) {  // ~7 % of pixels are stars
        const uint8_t phase = static_cast<uint8_t>(hash >> 24);
        const uint8_t tw = sin_u8(static_cast<uint8_t>(phase + t_ms / 45));
        if (tw > 40u) {
          frame.set(static_cast<int16_t>(x), static_cast<int16_t>(y),
                    hsv2rgb(150, 30, static_cast<uint8_t>(tw > 200u ? 255u : tw)));
        }
      }
    }
  }
}

// Ambient: rainbow_flow — full-height rainbow bands streaming sideways.
void render_rainbow_flow(FrameBuffer& frame, uint32_t t_ms) {
  frame.clear();
  for (uint8_t x = 0; x < kW; ++x) {
    const uint8_t hue = static_cast<uint8_t>(x * 8 + t_ms / 6);
    for (uint8_t y = 0; y < kH; ++y) {
      const uint8_t val = static_cast<uint8_t>(255u - static_cast<uint16_t>(y) * 14u);
      frame.set(static_cast<int16_t>(x), static_cast<int16_t>(y), hsv2rgb(hue, 255, val));
    }
  }
}

// Ambient: fire — deterministic flame ramp from the bottom edge.
void render_fire(FrameBuffer& frame, uint32_t t_ms) {
  frame.clear();
  for (int16_t y = kH - 1; y >= 0; --y) {
    const uint16_t row_from_bottom = static_cast<uint16_t>(kH - 1 - y);
    for (uint8_t x = 0; x < kW; ++x) {
      const uint8_t n =
          static_cast<uint8_t>(sin_u8(static_cast<uint8_t>(x * 29 + row_from_bottom * 11 +
                                                       t_ms / 45)) /
                               2u +
                               sin_u8(static_cast<uint8_t>(x * 7 - t_ms / 23)) / 2u);
      const uint16_t heat = (static_cast<uint16_t>(n) * (10u - row_from_bottom)) / 10u;
      if (heat > 46u) {
        frame.set(x, y, hsv2rgb(static_cast<uint8_t>(18 + row_from_bottom * 6), 240,
                                static_cast<uint8_t>(heat)));
      }
    }
  }
}

// Ambient: matrix — falling green character streams.
void render_matrix(FrameBuffer& frame, uint32_t t_ms) {
  frame.clear();
  for (uint8_t x = 0; x < kW; ++x) {
    const uint8_t seed = static_cast<uint8_t>(x * 53 + 7);
    const uint32_t cycle_ms = 500u + (seed % 400u);
    const uint32_t tau = t_ms % cycle_ms;
    int16_t head = static_cast<int16_t>(tau / 22);
    for (uint8_t trail = 0; trail < 5; ++trail) {
      const int16_t y = static_cast<int16_t>(head - trail);
      if (y < 0 || y > kH - 1) {
        continue;
      }
      const uint8_t val = static_cast<uint8_t>(255u >> (trail == 0 ? 0 : trail));
      frame.set(static_cast<int16_t>(x), y, hsv2rgb(110, 255, val));
    }
  }
}

// Ambient: calm — slow breathing cool field (soft idle pattern).
void render_calm(FrameBuffer& frame, uint32_t t_ms) {
  frame.clear();
  const uint8_t breath = sin_u8(static_cast<uint8_t>((t_ms / 36) & 0xFF));  // ~4.6 s cycle
  for (uint8_t y = 0; y < kH; ++y) {
    for (uint8_t x = 0; x < kW; ++x) {
      const int16_t dx = static_cast<int16_t>(x) - 15;
      const int16_t dy = static_cast<int16_t>(y) - 4;
      const uint16_t dist2 = static_cast<uint16_t>(dx * dx + dy * dy * 4);
      uint16_t val = static_cast<uint16_t>(breath) * 160u / 255u;
      val = (val > dist2 / 5u) ? val - dist2 / 5u : 0u;
      if (val > 0) {
        frame.set(static_cast<int16_t>(x), static_cast<int16_t>(y),
                  hsv2rgb(150, 120, static_cast<uint8_t>(val)));
      }
    }
  }
}

// ------------------------------------------------------- LED-L010 both-set
// Matrix counterparts of the tail/combined animations (ADR-LED-L010 D3).
// Timing constants are shared with the tail renderers so a target-"both"
// program reads as ONE visual across the two surfaces.

namespace {

// Radial distance helper for the pulse matrix counterpart.
inline uint16_t radial_dist2(int16_t x, int16_t y) {
  const int16_t dx = static_cast<int16_t>(x) - 15;
  const int16_t dy = static_cast<int16_t>(y) - 4;
  return static_cast<uint16_t>(dx * dx + dy * dy * 4);
}

constexpr Rgb kHeartRedMatrix{255, 40, 70};

// Heart predicate shared with the icon renderers (same anonymous-namespace
// formula semantics; duplicated as a local because the icon helper is
// file-private).
bool heart_pixel_local(int16_t x, int16_t y, float scale) {
  const float nx = static_cast<float>(x - 15) / (8.5f * scale);
  const float ny = static_cast<float>(4 - y) / (4.2f * scale) + 0.35f;
  const float a = nx * nx + ny * ny - 1.0f;
  return a * a * a - nx * nx * ny * ny * ny <= 0.0f;
}

}  // namespace

// pulse (matrix) — radial amber pulse ring from the center.
void render_pulse_matrix(FrameBuffer& frame, uint32_t t_ms) {
  frame.clear();
  const uint8_t wave = sin_u8(static_cast<uint8_t>((t_ms / 9) & 0xFF));  // ~2.3 s
  const uint16_t val = 40u + (static_cast<uint16_t>(wave) * 215u) / 255u;
  for (uint8_t y = 0; y < kH; ++y) {
    for (uint8_t x = 0; x < kW; ++x) {
      const uint16_t d2 = radial_dist2(static_cast<int16_t>(x), static_cast<int16_t>(y));
      uint16_t v = val;
      v = (v > d2 / 3u) ? v - d2 / 3u : 0u;
      if (v > 0) {
        frame.set(static_cast<int16_t>(x), static_cast<int16_t>(y),
                  hsv2rgb(30, 230, static_cast<uint8_t>(v)));
      }
    }
  }
}

// breathing (matrix) — subtle whole-field cool breathing.
void render_breathing_matrix(FrameBuffer& frame, uint32_t t_ms) {
  frame.clear();
  const uint8_t wave = sin_u8(static_cast<uint8_t>((t_ms / 36) & 0xFF));  // ~4.6 s
  const uint16_t val = 12u + (static_cast<uint16_t>(wave) * 78u) / 255u;
  frame.fill_rect(0, 0, kW, kH, hsv2rgb(150, 90, static_cast<uint8_t>(val)));
}

// lightning (matrix) — dark with a 3-stroke white-blue flash every ~2.4 s
// (same window constants as tail_lightning_core).
void render_lightning_matrix(FrameBuffer& frame, uint32_t t_ms) {
  frame.clear();
  const uint32_t phase = t_ms % 2400;
  if (phase >= 300 || (phase % 100) >= 45) {
    return;
  }
  const Rgb flash{210, 220, 255};
  frame.fill_rect(0, 0, kW, kH, flash);
  // A couple of dark "branch" pixels for texture (deterministic).
  frame.set(8, 3, kRgbBlack);
  frame.set(21, 6, kRgbBlack);
}

// progress (matrix) — bottom-up fill bar sweeping 0→100 % on a 5 s loop.
void render_progress_matrix(FrameBuffer& frame, uint32_t t_ms) {
  frame.clear();
  const uint8_t frac = static_cast<uint8_t>((t_ms % 5000) * 255u / 5000u);
  const uint8_t fill_rows =
      static_cast<uint8_t>((static_cast<uint16_t>(frac) * kH) / 255u);
  for (uint8_t y = 0; y < kH; ++y) {
    const bool filled = static_cast<uint8_t>(kH - 1 - y) < fill_rows;
    for (uint8_t x = 0; x < kW; ++x) {
      frame.set(static_cast<int16_t>(x), static_cast<int16_t>(y),
                filled ? Rgb{60, 230, 90} : Rgb{12, 30, 16});
    }
  }
}

// countdown_progress (matrix) — 10 s drain bar, green → orange → red, blink
// at zero (same constants as the tail counterpart).
void render_countdown_progress_matrix(FrameBuffer& frame, uint32_t t_ms) {
  frame.clear();
  const uint32_t tau = t_ms % 10000;
  const uint8_t frac = static_cast<uint8_t>(255u - (tau * 255u) / 10000u);
  Rgb fg{60, 230, 90};
  if (frac < 100) {
    fg = Rgb{230, 60, 50};
  } else if (frac < 150) {
    fg = Rgb{240, 160, 40};
  }
  if (frac == 0 && (tau / 250) % 2 == 0) {
    return;  // blink-off at zero
  }
  const uint8_t fill_rows =
      static_cast<uint8_t>((static_cast<uint16_t>(frac) * kH) / 255u);
  for (uint8_t y = 0; y < kH; ++y) {
    const bool filled = static_cast<uint8_t>(kH - 1 - y) < fill_rows;
    for (uint8_t x = 0; x < kW; ++x) {
      frame.set(static_cast<int16_t>(x), static_cast<int16_t>(y),
                filled ? fg : Rgb{12, 30, 16});
    }
  }
}

// chase (matrix) — a lit column streaming left→right with a hue spread.
void render_chase_matrix(FrameBuffer& frame, uint32_t t_ms) {
  frame.clear();
  const uint8_t col =
      static_cast<uint8_t>((t_ms / 22) % (kW < 6 ? 1u : static_cast<unsigned>(kW - 5)));
  for (uint8_t k = 0; k < 6; ++k) {
    const uint8_t x = static_cast<uint8_t>(col + k);
    if (x >= kW) {
      continue;
    }
    const uint8_t val = static_cast<uint8_t>(k == 0 ? 255u : 200u >> k);
    for (uint8_t y = 0; y < kH; ++y) {
      frame.set(static_cast<int16_t>(x), static_cast<int16_t>(y),
                hsv2rgb(static_cast<uint8_t>(k * 24), 240, val));
    }
  }
}

// color_wave (matrix) — full-field horizontal rainbow (no vertical fade;
// distinct from rainbow_flow's band form).
void render_color_wave_matrix(FrameBuffer& frame, uint32_t t_ms) {
  frame.clear();
  for (uint8_t x = 0; x < kW; ++x) {
    const uint8_t hue = static_cast<uint8_t>(x * 8 + t_ms / 10);
    for (uint8_t y = 0; y < kH; ++y) {
      frame.set(static_cast<int16_t>(x), static_cast<int16_t>(y), hsv2rgb(hue, 255, 220));
    }
  }
}

// heartbeat (matrix) — the heart formula scaling with the lub-dub cycle
// (1400 ms: lub at 0, dub at 240; dim rest in between).
void render_heartbeat_matrix(FrameBuffer& frame, uint32_t t_ms) {
  frame.clear();
  const uint32_t tau = t_ms % 1400;
  uint8_t intensity = 0;
  if (tau < 130) {
    intensity = static_cast<uint8_t>(255 - tau * 255 / 130);
  } else if (tau >= 240 && tau < 340) {
    intensity = static_cast<uint8_t>(255 - (tau - 240) * 255 / 100);
  }
  if (intensity == 0) {
    intensity = 26;  // dim resting heart so the shape stays readable
  }
  const float scale = 0.80f + 0.28f * (static_cast<float>(intensity) / 255.0f);
  for (uint8_t y = 0; y < kH; ++y) {
    for (uint8_t x = 0; x < kW; ++x) {
      if (heart_pixel_local(static_cast<int16_t>(x), static_cast<int16_t>(y), scale)) {
        frame.set(static_cast<int16_t>(x), static_cast<int16_t>(y), kHeartRedMatrix);
      }
    }
  }
}

AnimationRenderer animation_by_name(const char* name) {
  if (name == nullptr || name[0] == '\0') {
    return nullptr;  // fail closed: unknown binding renders blank upstream
  }
  // Registry names → fixed renderer bindings (internal data; frozen §7
  // entries first in registry order, then the LED-L009 additions).
  struct Binding {
    const char* name;
    AnimationRenderer renderer;
    bool time_varying;
  };
  static constexpr Binding kBindings[] = {
      {"corners", &render_corners, false},
      {"checker", &render_checker, true},
      {"border", &render_border, true},
      {"row_sweep", &render_row_sweep, true},
      {"bounce", &render_bounce, true},
      {"wave", &render_wave, true},
      {"rain", &render_rain, true},
      {"plasma", &render_plasma, true},
      {"smile", &render_smile, false},
      {"wink", &render_wink, true},
      {"heart", &render_heart, false},
      {"heart_pulse", &render_heart_pulse, true},
      {"eyes", &render_eyes, true},
      {"loading", &render_loading, true},
      {"success", &render_success, false},
      {"warning", &render_warning, false},
      {"arrows", &render_arrows, true},
      {"aurora", &render_aurora, true},
      {"stars", &render_stars, true},
      {"rainbow_flow", &render_rainbow_flow, true},
      {"fire", &render_fire, true},
      {"matrix", &render_matrix, true},
      {"calm", &render_calm, true},
      // LED-L010 both-surface set (matrix counterparts of the tail library;
      // the five tail-only names have no matrix binding — a Matrix-target
      // request for them is rejected upstream as unsupported).
      {"pulse", &render_pulse_matrix, true},
      {"breathing", &render_breathing_matrix, true},
      {"lightning", &render_lightning_matrix, true},
      {"progress", &render_progress_matrix, true},
      {"countdown_progress", &render_countdown_progress_matrix, true},
      {"chase", &render_chase_matrix, true},
      {"color_wave", &render_color_wave_matrix, true},
      {"heartbeat", &render_heartbeat_matrix, true},
  };
  for (const Binding& b : kBindings) {
    if (std::strcmp(b.name, name) == 0) {
      return b.renderer;
    }
  }
  return nullptr;
}

bool animation_is_time_varying(const char* name) {
  if (name == nullptr || name[0] == '\0') {
    return false;  // unknown name: static blank
  }
  if (combined_animation_name(name)) return true;
  // LED-L010: the five tail-only names have no matrix binding but animate on
  // the 1D surface — the render gate must keep re-rendering them.
  if (animation_by_name(name) == nullptr) {
    return tail_animation_by_name(name) != nullptr;
  }
  struct Binding {
    const char* name;
    bool time_varying;
  };
  static constexpr Binding kTimeVarying[] = {
      {"corners", false},   {"smile", false},  {"heart", false},
      {"success", false},   {"warning", false},
  };
  for (const Binding& b : kTimeVarying) {
    if (std::strcmp(b.name, name) == 0) {
      return false;
    }
  }
  return true;
}

}  // namespace effects
}  // namespace ledwall
