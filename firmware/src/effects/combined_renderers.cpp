#include "effects/combined_renderers.h"

#include <cstring>

#include "effects/animation_renderers.h"
#include "render/body_topology.h"
#include "render/color.h"

namespace ledwall {
namespace effects {

namespace {

constexpr uint32_t kTailAscentMs = 2400;
constexpr uint32_t kMatrixAscentMs = 350;
constexpr uint32_t kBurstMs = 800;
constexpr uint32_t kFadeMs = 900;
constexpr uint32_t kCycleMs = 4800;

uint8_t fade_value(uint32_t elapsed, uint32_t duration, uint8_t peak) {
  if (elapsed >= duration) return 0;
  return static_cast<uint8_t>((static_cast<uint32_t>(peak) * (duration - elapsed)) / duration);
}

Rgb warm_white(uint8_t value) { return Rgb{value, static_cast<uint8_t>(value * 230u / 255u),
                                            static_cast<uint8_t>(value * 190u / 255u)}; }

void render_fireworks(FrameBuffer& matrix, TailFramebuffer& tail, uint32_t elapsed_ms) {
  matrix.clear();
  tail.clear();
  const uint32_t cycle = elapsed_ms / kCycleMs;
  const uint32_t phase = elapsed_ms % kCycleMs;
  const uint16_t n = tail.length();
  const BodyTopology body;
  // A single rocket crosses the logical free end -> tail -> anchor -> matrix
  // on one clock. The two driver latches are not claimed to be simultaneous.
  if (phase < kTailAscentMs) {
    const uint16_t progress = static_cast<uint16_t>(
        (static_cast<uint64_t>(phase) * 56000u) / kTailAscentMs);
    const BodyPoint rocket = body.ascent(progress, n, 14, 4);
    const uint16_t head = rocket.tail_index;
    for (uint8_t i = 0; i < 4; ++i) {
      const uint16_t cell = static_cast<uint16_t>(head + i);
      if (cell < n) tail.set(cell, warm_white(static_cast<uint8_t>(255u - i * 48u)));
    }
    if (phase > kTailAscentMs - 160u) {
      body.light_anchor(matrix, warm_white(110));
    }
    return;
  }

  // The target stays inside the readable middle of a 30x10 panel. Cycling
  // through five sites provides variety without unbounded particle state.
  constexpr int8_t kBurstX[] = {7, 12, 18, 23, 15};
  constexpr int8_t kBurstY[] = {4, 3, 4, 3, 2};
  const int16_t bx = kBurstX[cycle % 5u];
  const int16_t by = kBurstY[cycle % 5u];
  const uint8_t hue = static_cast<uint8_t>(cycle * 47u + 12u);

  if (phase < kTailAscentMs + kMatrixAscentMs) {
    const uint32_t a = phase - kTailAscentMs;
    const uint16_t progress = static_cast<uint16_t>(56000u +
        (static_cast<uint64_t>(a) * (65535u - 56000u)) / kMatrixAscentMs);
    const BodyPoint rocket = body.ascent(progress, n, bx, by);
    const int16_t x = rocket.x;
    const int16_t y = rocket.y;
    matrix.set(x, y, warm_white(255));
    matrix.set(x, static_cast<int16_t>(y + 1), warm_white(170));
    matrix.set(x, static_cast<int16_t>(y + 2), warm_white(105));
    tail.set(0, warm_white(fade_value(a, kMatrixAscentMs, 170)));
    return;
  }

  const uint32_t burst_elapsed = phase - kTailAscentMs - kMatrixAscentMs;
  if (burst_elapsed >= kBurstMs + kFadeMs) return;
  // Small, deterministic 10-pixel-high burst. Positions are bounded by the
  // framebuffer; every particle is derived from the same cycle/phase.
  constexpr int8_t kDx[] = {-2, -2, -1, 0, 1, 2, 2, 2, 1, 0, -1, -2};
  constexpr int8_t kDy[] = {0, -1, -2, -2, -2, -1, 0, 1, 2, 2, 2, 1};
  const uint8_t val = burst_elapsed < kBurstMs
                          ? fade_value(burst_elapsed, kBurstMs + kFadeMs, 255)
                          : fade_value(burst_elapsed - kBurstMs, kFadeMs, 165);
  const int16_t radius = static_cast<int16_t>(1u +
      (burst_elapsed < kBurstMs ? burst_elapsed * 4u / kBurstMs : 4u));
  matrix.set(bx, by, hsv2rgb(hue, 180, val));
  for (uint8_t i = 0; i < 12; ++i) {
    const int16_t x = static_cast<int16_t>(bx + kDx[i] * radius / 2);
    const int16_t y = static_cast<int16_t>(by + kDy[i] * radius / 2 +
                         (burst_elapsed > kBurstMs ? (burst_elapsed - kBurstMs) / 360u : 0u));
    matrix.set(x, y, hsv2rgb(static_cast<uint8_t>(hue + i * 9u), 220, val));
  }
}

void render_meteor_impact(FrameBuffer& matrix, TailFramebuffer& tail, uint32_t t) {
  matrix.clear(); tail.clear();
  const uint32_t p = t % 3000u;
  if (p < 1200u) {
    const int16_t x = static_cast<int16_t>(2u + p * 12u / 1200u);
    const int16_t y = static_cast<int16_t>(p * 9u / 1200u);
    matrix.set(x, y, warm_white(230));
    matrix.set(static_cast<int16_t>(x - 1), static_cast<int16_t>(y - 1), Rgb{90, 110, 180});
    return;
  }
  const uint32_t shock = p - 1200u;
  const uint8_t glow = fade_value(shock, 1800u, 210);
  matrix.set(14, 9, Rgb{glow, static_cast<uint8_t>(glow / 3u), 0});
  matrix.set(15, 9, Rgb{glow, static_cast<uint8_t>(glow / 3u), 0});
  if (shock >= 1100u) return;
  const uint16_t head = static_cast<uint16_t>(shock * tail.length() / 1100u);
  for (uint8_t i = 0; i < 5; ++i) {
    const uint16_t cell = static_cast<uint16_t>(head + i);
    if (cell < tail.length()) tail.set(cell, Rgb{static_cast<uint8_t>(220u - i * 35u),
                                                static_cast<uint8_t>(85u - i * 12u), 10});
  }
}

void render_energy_bloom(FrameBuffer& matrix, TailFramebuffer& tail, uint32_t t) {
  matrix.clear(); tail.clear();
  const uint32_t p = t % 3600u;
  if (p < 1300u) {
    const uint16_t head = static_cast<uint16_t>((tail.length() - 1u) *
                                                (1300u - p) / 1300u);
    for (uint8_t i = 0; i < 5; ++i) {
      const uint16_t cell = static_cast<uint16_t>(head + i);
      if (cell < tail.length()) tail.set(cell, hsv2rgb(162, 210,
                                              static_cast<uint8_t>(210u - i * 32u)));
    }
    matrix.set(14, 9, hsv2rgb(162, 210, 50));
    matrix.set(15, 9, hsv2rgb(162, 210, 50));
    return;
  }
  render_heart(matrix, p);
  const uint8_t value = fade_value(p - 1300u, 2300u, 130);
  for (uint16_t i = 0; i < tail.length(); ++i) {
    const uint8_t v = static_cast<uint8_t>(value / (1u + i / 20u));
    tail.set(i, hsv2rgb(162, 210, v));
  }
}

void render_rainbow_river(FrameBuffer& matrix, TailFramebuffer& tail, uint32_t t) {
  matrix.clear(); tail.clear();
  const uint8_t phase = static_cast<uint8_t>(t / 35u);
  for (uint16_t i = 0; i < tail.length(); ++i) {
    tail.set(i, hsv2rgb(static_cast<uint8_t>(phase + i * 2u), 220, 115));
  }
  for (int16_t y = 0; y < 10; ++y) {
    for (int16_t x = 0; x < 30; ++x) {
      const int16_t dx = x < 14 ? static_cast<int16_t>(14 - x) :
                         (x > 15 ? static_cast<int16_t>(x - 15) : 0);
      const uint8_t distance = static_cast<uint8_t>((9 - y) * 5 + dx * 3);
      matrix.set(x, y, hsv2rgb(static_cast<uint8_t>(phase + distance), 220, 115));
    }
  }
}

void render_heartbeat_combined(FrameBuffer& matrix, TailFramebuffer& tail, uint32_t t) {
  render_heart(matrix, t);
  tail.clear();
  const uint32_t p = t % 1300u;
  for (uint8_t beat = 0; beat < 2; ++beat) {
    const uint32_t start = beat == 0 ? 0u : 260u;
    if (p < start || p >= start + 700u) continue;
    const uint32_t age = p - start;
    const uint16_t head = static_cast<uint16_t>(age * tail.length() / 700u);
    const uint8_t peak = beat == 0 ? 210u : 145u;
    const uint8_t value = fade_value(age, 700u, peak);
    for (uint8_t i = 0; i < 4; ++i) {
      const uint16_t cell = static_cast<uint16_t>(head + i);
      if (cell < tail.length()) tail.set(cell, Rgb{static_cast<uint8_t>(value / (i + 1u)),
                                                  0, static_cast<uint8_t>(value / 7u)});
    }
    matrix.set(14, 4, Rgb{peak, 0, 20});
    matrix.set(15, 4, Rgb{peak, 0, 20});
  }
}

void render_storm(FrameBuffer& matrix, TailFramebuffer& tail, uint32_t t) {
  matrix.clear(); tail.clear();
  const uint32_t p = t % 4200u;
  for (int16_t x = 4; x < 26; ++x) {
    matrix.set(x, 1, Rgb{30, 45, 75});
    if (x % 3 != 0) matrix.set(x, 2, Rgb{25, 38, 66});
  }
  if (p < 1800u || p >= 2250u) return;
  const uint32_t age = p - 1800u;
  const uint8_t flash = age < 170u ? 185u : fade_value(age - 170u, 280u, 160u);
  for (int16_t y = 2; y < 10; ++y) {
    const int16_t x = static_cast<int16_t>(14 + (y % 3 == 0 ? 1 : 0));
    matrix.set(x, y, warm_white(flash));
  }
  const uint16_t head = static_cast<uint16_t>(age * tail.length() / 450u);
  for (uint8_t i = 0; i < 3; ++i) {
    const uint16_t cell = static_cast<uint16_t>(head + i);
    if (cell < tail.length()) tail.set(cell, warm_white(static_cast<uint8_t>(flash / (i + 1u))));
  }
}

void render_lava_fountain(FrameBuffer& matrix, TailFramebuffer& tail, uint32_t t) {
  matrix.clear(); tail.clear();
  const uint32_t p = t % 2600u;
  const uint16_t head = static_cast<uint16_t>((tail.length() - 1u) *
                                              (p < 1600u ? 1600u - p : 0u) / 1600u);
  for (uint8_t i = 0; i < 12; ++i) {
    const uint16_t cell = static_cast<uint16_t>(head + i);
    if (cell < tail.length()) tail.set(cell, hsv2rgb(static_cast<uint8_t>(10u + i * 2u),
                                           245, static_cast<uint8_t>(170u - i * 11u)));
  }
  if (p < 1600u) return;
  const uint32_t age = p - 1600u;
  const uint8_t val = fade_value(age, 1000u, 210u);
  for (uint8_t i = 0; i < 8; ++i) {
    const int16_t x = static_cast<int16_t>(14 + static_cast<int16_t>((i * 7u + t / 140u) % 9u) - 4);
    const int16_t y = static_cast<int16_t>(9 - static_cast<int16_t>((age / 140u + i * 3u) % 8u));
    matrix.set(x, y, hsv2rgb(static_cast<uint8_t>(5u + i * 4u), 245, val));
  }
  matrix.set(14, 9, Rgb{170, 50, 0});
  matrix.set(15, 9, Rgb{170, 50, 0});
}

void render_rain_drain(FrameBuffer& matrix, TailFramebuffer& tail, uint32_t t) {
  matrix.clear(); tail.clear();
  constexpr int8_t kColumns[] = {3, 7, 11, 17, 23, 27};
  for (uint8_t i = 0; i < 6; ++i) {
    const uint32_t p = (t + i * 320u) % 2400u;
    if (p < 1000u) {
      const int16_t y = static_cast<int16_t>(p * 9u / 1000u);
      matrix.set(kColumns[i], y, Rgb{20, 100, 170});
    } else if (p < 2200u) {
      const uint16_t cell = static_cast<uint16_t>((p - 1000u) * tail.length() / 1200u);
      if (cell < tail.length()) tail.set(cell, Rgb{20, 85, 160});
      matrix.set(14, 9, Rgb{10, 50, 90});
      matrix.set(15, 9, Rgb{10, 50, 90});
    }
  }
}

void render_aurora_current(FrameBuffer& matrix, TailFramebuffer& tail, uint32_t t) {
  matrix.clear(); tail.clear();
  const uint8_t phase = static_cast<uint8_t>(t / 95u);
  for (uint16_t i = 0; i < tail.length(); ++i) {
    tail.set(i, hsv2rgb(static_cast<uint8_t>(phase + i / 3u), 175, 85));
  }
  for (int16_t y = 0; y < 10; ++y) {
    for (int16_t x = 0; x < 30; ++x) {
      const int16_t dx = x < 14 ? static_cast<int16_t>(14 - x) :
                         (x > 15 ? static_cast<int16_t>(x - 15) : 0);
      matrix.set(x, y, hsv2rgb(static_cast<uint8_t>(phase + (9 - y) * 5 + dx),
                               175, static_cast<uint8_t>(65u + (x + y + phase) % 28u)));
    }
  }
}

void render_starstream(FrameBuffer& matrix, TailFramebuffer& tail, uint32_t t) {
  matrix.clear(); tail.clear();
  constexpr int8_t kX[] = {9, 15, 21};
  constexpr int8_t kY[] = {3, 4, 2};
  for (uint8_t i = 0; i < 3; ++i) {
    const uint32_t p = (t + i * 650u) % 2400u;
    if (p < 1400u) {
      const uint16_t cell = static_cast<uint16_t>((tail.length() - 1u) *
                                                  (1400u - p) / 1400u);
      tail.set(cell, warm_white(180));
      if (cell + 1u < tail.length()) tail.set(static_cast<uint16_t>(cell + 1u), warm_white(90));
    } else {
      const uint8_t value = fade_value(p - 1400u, 1000u, 180);
      matrix.set(kX[i], kY[i], warm_white(value));
      matrix.set(static_cast<int16_t>(kX[i] - 1), kY[i], warm_white(static_cast<uint8_t>(value / 3u)));
      matrix.set(static_cast<int16_t>(kX[i] + 1), kY[i], warm_white(static_cast<uint8_t>(value / 3u)));
      matrix.set(kX[i], static_cast<int16_t>(kY[i] - 1), warm_white(static_cast<uint8_t>(value / 3u)));
      matrix.set(kX[i], static_cast<int16_t>(kY[i] + 1), warm_white(static_cast<uint8_t>(value / 3u)));
    }
  }
}

}  // namespace

bool combined_animation_name(const char* name) {
  if (name == nullptr) return false;
  constexpr const char* kNames[] = {
      "fireworks", "meteor_impact", "energy_bloom", "rainbow_river", "heartbeat",
      "storm", "lava_fountain", "rain_drain", "aurora_current", "starstream"};
  for (const char* entry : kNames) {
    if (std::strcmp(name, entry) == 0) return true;
  }
  return false;
}

bool render_combined(const char* name, FrameBuffer& matrix,
                     TailFramebuffer& tail, uint32_t elapsed_ms) {
  if (name == nullptr) return false;
  if (std::strcmp(name, "fireworks") == 0) {
    render_fireworks(matrix, tail, elapsed_ms);
    return true;
  }
  if (std::strcmp(name, "meteor_impact") == 0) { render_meteor_impact(matrix, tail, elapsed_ms); return true; }
  if (std::strcmp(name, "energy_bloom") == 0) { render_energy_bloom(matrix, tail, elapsed_ms); return true; }
  if (std::strcmp(name, "rainbow_river") == 0) { render_rainbow_river(matrix, tail, elapsed_ms); return true; }
  if (std::strcmp(name, "heartbeat") == 0) { render_heartbeat_combined(matrix, tail, elapsed_ms); return true; }
  if (std::strcmp(name, "storm") == 0) { render_storm(matrix, tail, elapsed_ms); return true; }
  if (std::strcmp(name, "lava_fountain") == 0) { render_lava_fountain(matrix, tail, elapsed_ms); return true; }
  if (std::strcmp(name, "rain_drain") == 0) { render_rain_drain(matrix, tail, elapsed_ms); return true; }
  if (std::strcmp(name, "aurora_current") == 0) { render_aurora_current(matrix, tail, elapsed_ms); return true; }
  if (std::strcmp(name, "starstream") == 0) { render_starstream(matrix, tail, elapsed_ms); return true; }
  return false;
}

}  // namespace effects
}  // namespace ledwall
