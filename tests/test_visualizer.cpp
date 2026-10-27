#include "test_framework.h"

#include "visualizer/spectrum_input.h"

// LED-L009 external visualizer input: bounds, malformed input, rate limit,
// expiry fail-safe, deterministic decay.

using namespace ledwall;

namespace {
void fill(uint8_t* b, uint8_t v) {
  for (uint8_t i = 0; i < kSpectrumBands; ++i) b[i] = v;
}
}  // namespace

void test_visualizer_accepts_wellformed_feed() {
  SpectrumInput s;
  uint8_t bands[kSpectrumBands];
  fill(bands, 128);
  CHECK(s.feed(bands, kSpectrumBands, 1000));
  CHECK(s.active(1000));
  CHECK_EQ_INT(s.band(0, 1000), 128);
  CHECK_EQ_INT(s.band(29, 1000), 128);
  CHECK_EQ_INT(s.band(30, 1000), 0);  // out-of-range band reads zero (bounds-safe)
}

void test_visualizer_rejects_malformed_and_out_of_rate() {
  SpectrumInput s;
  uint8_t bands[kSpectrumBands];
  fill(bands, 200);
  CHECK(s.feed(bands, kSpectrumBands, 1000));
  // Wrong shape mutates nothing.
  CHECK(!s.feed(bands, kSpectrumBands - 1, 1100));
  CHECK(!s.feed(nullptr, kSpectrumBands, 1100));
  CHECK_EQ_INT(s.band(0, 1100), 200);
  // Rate limit: < 25 ms since the last accepted feed.
  CHECK(!s.feed(bands, kSpectrumBands, 1020));
  CHECK(s.feed(bands, kSpectrumBands, 1025));
}

void test_visualizer_expiry_and_decay() {
  SpectrumInput s;
  uint8_t bands[kSpectrumBands];
  fill(bands, 100);
  CHECK(s.feed(bands, kSpectrumBands, 0));
  CHECK(s.active(SpectrumInput::kTimeoutMs - 1));
  CHECK(!s.active(SpectrumInput::kTimeoutMs));  // stale: inactive at the timeout boundary
  // Decay begins after the timeout: 2000/s drop.
  s.tick(SpectrumInput::kTimeoutMs + 500);
  CHECK_EQ_INT(s.band(0, SpectrumInput::kTimeoutMs + 500), 0);  // 100 < 1000 drop → zero
  fill(bands, 255);
  CHECK(s.feed(bands, kSpectrumBands, 10000));
  // Decay runs at 2000/s: 125 ms past the timeout → drop 250 → 5 left.
  s.tick(10000 + SpectrumInput::kTimeoutMs + 125);
  CHECK_EQ_INT(s.band(0, 10000 + SpectrumInput::kTimeoutMs + 125), 5);
  s.tick(10000 + SpectrumInput::kTimeoutMs + 1000);  // cumulative decay to zero
  CHECK_EQ_INT(s.band(0, 10000 + SpectrumInput::kTimeoutMs + 1000), 0);
  CHECK(!s.active(10000 + SpectrumInput::kTimeoutMs + 1000));  // fully decayed → inactive
}

void test_visualizer_clear() {
  SpectrumInput s;
  uint8_t bands[kSpectrumBands];
  fill(bands, 255);
  CHECK(s.feed(bands, kSpectrumBands, 0));
  s.clear();
  CHECK(!s.active(0));
  CHECK_EQ_INT(s.band(0, 0), 0);
}
