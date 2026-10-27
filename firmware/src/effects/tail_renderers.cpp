#include "effects/tail_renderers.h"

#include <cstring>

#include "render/color.h"

namespace ledwall {
namespace effects {

namespace {

// Deterministic per-cell hash (same construction as the matrix star field —
// no PRNG state anywhere).
inline uint32_t cell_hash(uint16_t cell, uint32_t salt) {
  return 2654435761u * (1u + cell + 31u * salt);
}

// Triangle wave over [0, span): 0→span→0 (scanner bounce). A degenerate
// span (0) is clamped to position 0 — never a division by zero.
inline uint16_t triangle_u16(uint32_t x, uint16_t span) {
  if (span == 0) {
    return 0;
  }
  const uint32_t m = x % (2u * static_cast<uint32_t>(span));
  return static_cast<uint16_t>(m < span ? m : 2u * span - m);
}

constexpr uint8_t kHueRed = 0;
constexpr uint8_t kHueCoolWhite = 150;

}  // namespace

uint16_t tail_scanner_position(uint16_t length, uint32_t t_ms) {
  if (length == 0) {
    return 0;
  }
  return triangle_u16(t_ms / 200, static_cast<uint16_t>(length - 1));
}

// comet — bright warm head with a long fading trail, wrapping.
void tail_render_comet(TailFramebuffer& tail, uint32_t t_ms) {
  tail.clear();
  const uint16_t len = tail.length();
  const uint16_t head = static_cast<uint16_t>((t_ms / 16) % len);
  const uint16_t trail_len = static_cast<uint16_t>(len / 4 + 6);
  for (uint16_t k = 0; k < trail_len; ++k) {
    const uint16_t cell = static_cast<uint16_t>(head + len - k) % len;
    const uint8_t val = static_cast<uint8_t>(k == 0 ? 255u : 230u / (k + 1));
    tail.set(cell, hsv2rgb(28, 200, val));
  }
}

// scanner — a single bright pixel bouncing end-to-end for bounded Tail tests.
// The 200 ms dwell exceeds the 50 ms frame floor so no logical position is skipped.
void tail_render_scanner(TailFramebuffer& tail, uint32_t t_ms) {
  tail.clear();
  const uint16_t center = tail_scanner_position(tail.length(), t_ms);
  tail.set(center, Rgb{220, 245, 255});
}

// meteor — fast white-hot head, flickering sparks, longer smoky trail.
void tail_render_meteor(TailFramebuffer& tail, uint32_t t_ms) {
  tail.clear();
  const uint16_t len = tail.length();
  const uint16_t head = static_cast<uint16_t>((t_ms / 9) % len);
  tail.set(head, Rgb{255, 250, 240});
  if (head >= 1) {
    tail.set(static_cast<uint16_t>(head - 1), Rgb{255, 210, 160});
  }
  const uint16_t trail_len = static_cast<uint16_t>(len / 3 + 8);
  for (uint16_t k = 2; k < trail_len; ++k) {
    const uint16_t cell = static_cast<uint16_t>(head + len - k) % len;
    const uint8_t val = static_cast<uint8_t>(150u / (k / 2 + 1));
    tail.set(cell, hsv2rgb(20, 180, val));
  }
  // Deterministic spark flicker around the head (two cells, hash-gated).
  const uint8_t flick = static_cast<uint8_t>(cell_hash(t_ms / 55, 7) & 0xFF);
  if (flick > 128) {
    const uint16_t s = static_cast<uint16_t>(head + len + 2) % len;
    tail.set(s, Rgb{255, 230, 200});
  }
}

// spark_trail — four independent sparks at different speeds/directions.
void tail_render_spark_trail(TailFramebuffer& tail, uint32_t t_ms) {
  tail.clear();
  const uint16_t len = tail.length();
  const uint8_t hues[4] = {0, 70, 140, 200};
  const uint8_t speeds[4] = {13, 27, 41, 59};  // ms per cell step
  for (uint8_t s = 0; s < 4; ++s) {
    const uint16_t phase = static_cast<uint16_t>((t_ms / speeds[s]) % len);
    // Two sparks wrap upward, two travel downward for cross-motion.
    const bool downward = (s & 1) != 0;
    const uint16_t cell = downward ? static_cast<uint16_t>(len - 1 - phase) : phase;
    for (uint16_t k = 0; k < 5; ++k) {
      const uint16_t off = downward ? k : static_cast<uint16_t>(len - k);
      const uint16_t c = static_cast<uint16_t>((cell + off) % len);
      const uint8_t val = static_cast<uint8_t>(k == 0 ? 255u : 200u >> k);
      tail.set(c, hsv2rgb(hues[s], 220, val));
    }
  }
}

// fire_flow — heat ramp flowing along the tail (fire palette, one end hotter).
void tail_render_fire_flow(TailFramebuffer& tail, uint32_t t_ms) {
  tail.clear();
  const uint16_t len = tail.length();
  for (uint16_t x = 0; x < len; ++x) {
    const uint8_t n =
        static_cast<uint8_t>(sin_u8(static_cast<uint8_t>(x * 23 + t_ms / 30)) / 2u +
                             sin_u8(static_cast<uint8_t>(x * 11 - t_ms / 17)) / 2u);
    // Flow: heat grows toward cell 0 and drifts with time.
    const uint16_t heat = (static_cast<uint16_t>(n) * (len - x)) / (len > 0 ? len : 1);
    if (heat > 40) {
      tail.set(x, hsv2rgb(static_cast<uint8_t>(12 + (x * 24u) / (len > 0 ? len : 1)), 240,
                          static_cast<uint8_t>(heat)));
    }
  }
}

// ---- parameterized cores ----

void tail_pulse_core(TailFramebuffer& tail, uint32_t t_ms, uint8_t hue, uint8_t val_min,
                     uint32_t period_ms_divisor) {
  tail.clear();
  const uint8_t wave = sin_u8(static_cast<uint8_t>((t_ms / period_ms_divisor) & 0xFF));
  const uint16_t val = val_min + (static_cast<uint16_t>(wave) * (255u - val_min)) / 255u;
  tail.fill(hsv2rgb(hue, 230, static_cast<uint8_t>(val)));
}

void tail_progress_core(TailFramebuffer& tail, uint8_t frac, Rgb fg, Rgb bg) {
  tail.clear();
  const uint16_t len = tail.length();
  const uint16_t fill = static_cast<uint16_t>((static_cast<uint32_t>(frac) * len) / 255u);
  tail.fill(bg);
  for (uint16_t x = 0; x < fill && x < len; ++x) {
    tail.set(x, fg);
  }
}

void tail_lightning_core(TailFramebuffer& tail, uint32_t t_ms, uint32_t window_ms) {
  tail.clear();
  const uint32_t phase = t_ms % window_ms;
  // Three quick strokes inside the first 300 ms of each window.
  if (phase >= 300) {
    return;
  }
  const bool stroke_on = (phase % 100) < 45;  // 3 × 45 ms on / 55 ms off
  if (!stroke_on) {
    return;
  }
  const uint16_t len = tail.length();
  for (uint16_t x = 0; x < len; ++x) {
    const uint8_t jitter = static_cast<uint8_t>(200u + (cell_hash(x, t_ms / 45) % 56u));
    tail.set(x, Rgb{jitter, jitter, 255});
  }
}

void tail_chase_core(TailFramebuffer& tail, uint32_t t_ms, uint8_t hue_base) {
  tail.clear();
  const uint16_t len = tail.length();
  const uint16_t gap = static_cast<uint16_t>(len < 24 ? 4 : len / 6);
  for (uint16_t k = 0; k < 6; ++k) {
    const uint16_t cell =
        static_cast<uint16_t>(((t_ms / 22) + k * gap) % len);
    tail.set(cell, hsv2rgb(static_cast<uint8_t>(hue_base + k * 24), 240, 255));
  }
}

// ---- registered animations ----

// pulse — whole-tail color pulse (combined: matrix radial pulse counterpart).
void tail_render_pulse(TailFramebuffer& tail, uint32_t t_ms) {
  tail_pulse_core(tail, t_ms, 30, 40, 9);  // warm amber, ~2.3 s cycle
}

// breathing — slow, low-amplitude cool wash.
void tail_render_breathing(TailFramebuffer& tail, uint32_t t_ms) {
  tail_pulse_core(tail, t_ms, kHueCoolWhite, 12, 36);  // ~4.6 s cycle
}

// lightning — dark tail with a 3-stroke flash every ~2.4 s.
void tail_render_lightning(TailFramebuffer& tail, uint32_t t_ms) {
  tail_lightning_core(tail, t_ms, 2400);
}

// progress — demo fill sweeping 0→100 % on a 5 s loop (combined timer uses
// the parameterized core with live timer truth).
void tail_render_progress(TailFramebuffer& tail, uint32_t t_ms) {
  const uint8_t frac = static_cast<uint8_t>((t_ms % 5000) * 255u / 5000u);
  tail_progress_core(tail, frac, Rgb{60, 230, 90}, Rgb{12, 30, 16});
}

// countdown_progress — demo drain 100%→0 on a 10 s loop; green → orange →
// red; blink at zero (combined timer uses the live remaining fraction).
void tail_render_countdown_progress(TailFramebuffer& tail, uint32_t t_ms) {
  const uint32_t tau = t_ms % 10000;
  const uint8_t frac = static_cast<uint8_t>(255u - (tau * 255u) / 10000u);
  Rgb fg{60, 230, 90};
  if (frac < 100) {
    fg = Rgb{230, 60, 50};
  } else if (frac < 150) {
    fg = Rgb{240, 160, 40};
  }
  if (frac == 0 && (tau / 250) % 2 == 0) {
    tail.clear();
    return;
  }
  tail_progress_core(tail, frac, fg, Rgb{12, 30, 16});
}

// chase — six evenly spaced dots streaming with a hue spread.
void tail_render_chase(TailFramebuffer& tail, uint32_t t_ms) {
  tail_chase_core(tail, t_ms, 0);
}

// color_wave — smooth rainbow flowing along the rope.
void tail_render_color_wave(TailFramebuffer& tail, uint32_t t_ms) {
  tail.clear();
  const uint16_t len = tail.length();
  for (uint16_t x = 0; x < len; ++x) {
    const uint8_t hue = static_cast<uint8_t>((static_cast<uint32_t>(x) * 255u) /
                                                 (len > 0 ? len : 1) +
                                             t_ms / 10);
    tail.set(x, hsv2rgb(hue, 255, 220));
  }
}

// heartbeat — lub-dub: two quick pulses, then rest (red).
void tail_render_heartbeat(TailFramebuffer& tail, uint32_t t_ms) {
  tail.clear();
  const uint32_t tau = t_ms % 1400;
  uint8_t intensity = 0;
  if (tau < 130) {
    intensity = static_cast<uint8_t>(255 - tau * 255 / 130);          // lub
  } else if (tau >= 240 && tau < 340) {
    intensity = static_cast<uint8_t>(255 - (tau - 240) * 255 / 100);  // dub
  }
  if (intensity == 0) {
    tail.fill(Rgb{26, 0, 4});  // dim resting red so the rope reads alive
    return;
  }
  tail.fill(hsv2rgb(kHueRed, 250, intensity));
}

// rainbow_flow (tail form) — the existing ambient name on the 1D surface.
void tail_render_rainbow_flow(TailFramebuffer& tail, uint32_t t_ms) {
  tail.clear();
  const uint16_t len = tail.length();
  for (uint16_t x = 0; x < len; ++x) {
    const uint8_t hue = static_cast<uint8_t>(x * 4 + t_ms / 6);
    tail.set(x, hsv2rgb(hue, 255, 230));
  }
}

TailRenderer tail_animation_by_name(const char* name) {
  if (name == nullptr || name[0] == '\0') {
    return nullptr;
  }
  struct TailBinding {
    const char* name;
    TailRenderer renderer;
  };
  static constexpr TailBinding kBindings[] = {
      {"comet", &tail_render_comet},
      {"scanner", &tail_render_scanner},
      {"pulse", &tail_render_pulse},
      {"breathing", &tail_render_breathing},
      {"meteor", &tail_render_meteor},
      {"spark_trail", &tail_render_spark_trail},
      {"lightning", &tail_render_lightning},
      {"fire_flow", &tail_render_fire_flow},
      {"progress", &tail_render_progress},
      {"countdown_progress", &tail_render_countdown_progress},
      {"chase", &tail_render_chase},
      {"color_wave", &tail_render_color_wave},
      {"heartbeat", &tail_render_heartbeat},
      {"rainbow_flow", &tail_render_rainbow_flow},
  };
  for (const TailBinding& b : kBindings) {
    if (std::strcmp(b.name, name) == 0) {
      return b.renderer;
    }
  }
  return nullptr;
}

}  // namespace effects
}  // namespace ledwall
