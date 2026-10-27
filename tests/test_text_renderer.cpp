#include "render/color.h"
#include "render/framebuffer.h"
#include "text/text_renderer.h"
#include "test_framework.h"

// Text pipeline goldens (frozen §6). Expected frames are hand-derived from the
// heritage glyph table, the 6 px advance, the centered-static rule, and the
// heritage scroll model offset(t) = (t/speed) % (w+30) − w at anchor y=2 —
// never from the implementation under test.

namespace {

constexpr uint8_t kW = 30;
constexpr uint8_t kH = 10;

const ledwall::Rgb kText = ledwall::hsv2rgb(96, 200, 255);  // heritage text palette

struct Pixel {
  uint8_t x;
  uint8_t y;
};

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

bool lit(const ledwall::FrameBuffer& f, uint8_t x, uint8_t y) {
  return f.at(x, y) != ledwall::kRgbBlack;
}

}  // namespace

void test_text_width_and_skipped() {
  CHECK_EQ_INT(ledwall::text::string_width_px(""), 0);
  CHECK_EQ_INT(ledwall::text::string_width_px("HI"), 12);        // heritage n*6
  CHECK_EQ_INT(ledwall::text::string_width_px("HELLO"), 30);     // exactly fitting
  CHECK_EQ_INT(ledwall::text::string_width_px("HELLO WALL"), 60);
  CHECK_EQ_INT(ledwall::text::string_width_px(nullptr), 0);

  CHECK_EQ_INT(ledwall::text::count_skipped_characters("HI!"), 0);
  CHECK_EQ_INT(ledwall::text::count_skipped_characters(""), 0);
  // LED-L009 semantics: counting is per UTF-8 CODE POINT. 'ï' is one
  // unsupported code point (renders as one space, counts once); lowercase is
  // now fully supported (extended charset) and skips nothing.
  CHECK_EQ_INT(ledwall::text::count_skipped_characters("H\xC3\xAF!"), 1);
  CHECK_EQ_INT(ledwall::text::count_skipped_characters("hello"), 0);
  CHECK_EQ_INT(ledwall::text::count_skipped_characters("\xC3\x8B\xC3\xA7"), 0);  // Ëç supported

  CHECK(ledwall::text::fits_static("HELLO"));       // 30 px ≤ 30 px
  CHECK(!ledwall::text::fits_static("HELLO!"));     // 36 px > 30 px
  CHECK(ledwall::text::fits_static("HI"));
}

void test_text_static_centered_golden() {
  // "HI": width 12 → x0 = (30−12)/2 = 9, anchor y=2. Lit pixels derived by
  // hand from H={7F,08,08,08,7F}, I={00,41,7F,41,00}.
  ledwall::FrameBuffer f;
  ledwall::text::draw_static(f, "HI", kText);

  const Pixel expected[] = {
      // H at x=9..13 (col0/col4 full rows 2..8, cols1-3 mid row y=5)
      {9, 2}, {9, 3}, {9, 4}, {9, 5}, {9, 6}, {9, 7}, {9, 8},
      {10, 5}, {11, 5}, {12, 5},
      {13, 2}, {13, 3}, {13, 4}, {13, 5}, {13, 6}, {13, 7}, {13, 8},
      // I at x=15..19: col0 (x=15) is EMPTY; caps at x=16 and x=18, full
      // middle column at x=17.
      {16, 2}, {16, 8},
      {17, 2}, {17, 3}, {17, 4}, {17, 5}, {17, 6}, {17, 7}, {17, 8},
      {18, 2}, {18, 8},
  };
  CHECK_EQ_INT(count_lit(f), 28);
  for (const Pixel& p : expected) {
    CHECK(f.at(p.x, p.y) == kText);
  }
  // Advance gaps (x=14) and the empty I columns (x=15, x=19) stay black.
  CHECK(f.at(14, 5) == ledwall::kRgbBlack);
  CHECK(f.at(15, 5) == ledwall::kRgbBlack);
  CHECK(f.at(19, 5) == ledwall::kRgbBlack);
  CHECK(f.at(0, 0) == ledwall::kRgbBlack);
  CHECK(f.at(29, 9) == ledwall::kRgbBlack);
}

void test_text_static_exactly_fitting() {
  // "HELLO": 30 px on a 30 px canvas → starts at x0=0, fully visible.
  // Hand-counted glyph pixels: H=17, E=18, L=11, L=11, O=16 → 73 total.
  ledwall::FrameBuffer f;
  ledwall::text::draw_static(f, "HELLO", kText);
  CHECK(ledwall::text::fits_static("HELLO"));
  // H's full left column: col0 of H → x=0, rows 2..8.
  for (uint8_t y = 2; y <= 8; ++y) {
    CHECK(f.at(0, y) == kText);
  }
  // Last O's right column: O = {3E,41,41,41,3E}; glyph starts at x=24:
  // col4 = 0x3E (rows 1..5) → x=28, y=3..7.
  for (uint8_t y = 3; y <= 7; ++y) {
    CHECK(f.at(28, y) == kText);
  }
  CHECK_EQ_INT(count_lit(f), 73);
  // Advance gaps stay black (x=5..6 between H and E, x=23 before O).
  CHECK(f.at(5, 5) == ledwall::kRgbBlack);
  CHECK(f.at(23, 5) == ledwall::kRgbBlack);
}

void test_text_static_one_px_too_wide_is_rejected_upstream() {
  // The 6 px monospace advance makes widths jump in steps of 6: 30 px fits,
  // 36 px is the smallest violation. The §6 predicate is width > 30.
  CHECK(ledwall::text::fits_static("HELLO"));    // 30 px
  CHECK(!ledwall::text::fits_static("HELLOW"));  // 36 px (smallest too-wide)
}

void test_text_scroll_initial_and_first_step_golden() {
  ledwall::FrameBuffer f;
  // t=0: offset = −12 → the message is entirely off-canvas (enters tail-first).
  ledwall::text::draw_scrolling(f, "HI", kText, 0, 40);
  CHECK_EQ_INT(count_lit(f), 0);

  // t=40: offset = −11 → still fully off-canvas.
  ledwall::text::draw_scrolling(f, "HI", kText, 40, 40);
  CHECK_EQ_INT(count_lit(f), 0);

  // t=240: offset = (240/40)%42 − 12 = −6 → the tail glyph 'I' has its first
  // column on-canvas: I's col1 (x=1) rows 0/6, col2 (x=2) all, col3 (x=3).
  ledwall::text::draw_scrolling(f, "HI", kText, 240, 40);
  const Pixel expected[] = {
      {1, 2}, {1, 8},
      {2, 2}, {2, 3}, {2, 4}, {2, 5}, {2, 6}, {2, 7}, {2, 8},
      {3, 2}, {3, 8},
  };
  CHECK_EQ_INT(count_lit(f), 11);
  for (const Pixel& p : expected) {
    CHECK(f.at(p.x, p.y) == kText);
  }
}

void test_text_scroll_full_traversal_wraps() {
  // One full traversal = speed × span ms; the frame at wrap equals t=0.
  const char* msg = "HI";
  const uint32_t speed = 40;
  const uint32_t span = ledwall::text::scroll_span_px(msg);  // 12 + 30 = 42
  CHECK_EQ_INT(span, 42);

  ledwall::FrameBuffer at_zero, wrapped;
  ledwall::text::draw_scrolling(at_zero, msg, kText, 0, speed);
  ledwall::text::draw_scrolling(wrapped, msg, kText, speed * span, speed);
  CHECK(std::memcmp(at_zero.data(), wrapped.data(), 300 * sizeof(ledwall::Rgb)) == 0);

  // Mid-traversal frame is visible and deterministic.
  ledwall::FrameBuffer mid1, mid2;
  ledwall::text::draw_scrolling(mid1, msg, kText, 600, speed);
  ledwall::text::draw_scrolling(mid2, msg, kText, 600, speed);
  CHECK(std::memcmp(mid1.data(), mid2.data(), 300 * sizeof(ledwall::Rgb)) == 0);
  CHECK(count_lit(mid1) > 0);
  // offset(600) = (600/40)%42 − 12 = 15−12 = 3 → "HI" at x=3..13:
  // H at 3..7 (col0 full at x=3), I's middle column (col2) at x=11.
  CHECK(lit(mid1, 3, 5));
  CHECK(lit(mid1, 11, 5));
  CHECK(!lit(mid1, 14, 5));  // advance gap after the message
}

void test_text_scroll_rollover_timing() {
  // Wrap-safe program-relative elapsed: started near the uint32 ceiling.
  // started = 0xFFFFFF00 ≡ −256 → at now = 0x314 (788) the elapsed is
  // 788 + 256 = 1044 (uint32 wraparound), NOT 788.
  ledwall::FrameBuffer a, b;
  const uint32_t started = 0xFFFFFF00u;
  const uint32_t now = 0x314u;
  ledwall::text::draw_scrolling(a, "HI", kText, now - started, 40);
  ledwall::text::draw_scrolling(b, "HI", kText, 1044, 40);
  CHECK(std::memcmp(a.data(), b.data(), 300 * sizeof(ledwall::Rgb)) == 0);
  // offset(1044) = (1044/40) % 42 − 12 = 26 − 12 = 14 → H at 14..18.
  CHECK(lit(a, 14, 5));  // H col0
  CHECK(lit(a, 18, 5));  // H col4
  CHECK(!lit(a, 13, 5));  // before the message
}
