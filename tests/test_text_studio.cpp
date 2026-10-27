#include "test_framework.h"

#include "render/color.h"
#include "render/renderer.h"
#include "text/font5x7.h"
#include "text/text_renderer.h"
#include "text/text_style.h"

// LED-L009 Text Studio: RTL marquee (primary reading direction), heritage LTR
// preserved, word-by-word dwell/loop/one-shot, loop-without-freezing, speed
// semantics, semantic styles/palette, extended charset + truthful
// skippedCharacters (code points).

using namespace ledwall;
using namespace ledwall::text;

void test_text_style_registry() {
  CHECK_EQ_INT(kTextStyleCount, 4);
  CHECK_STR_EQ(text_style_name(TextStyle::Solid), "solid");
  CHECK_STR_EQ(text_style_name(TextStyle::ColorBg), "color_bg");
  CHECK_STR_EQ(text_style_name(TextStyle::RainbowText), "rainbow_text");
  CHECK_STR_EQ(text_style_name(TextStyle::RainbowBg), "rainbow_bg");
  TextStyle st = TextStyle::Solid;
  CHECK(text_style_from_name("rainbow_bg", &st));
  CHECK(st == TextStyle::RainbowBg);
  CHECK(!text_style_from_name("sparkle", &st));
  CHECK(!text_style_from_name(nullptr, &st));
}

void test_text_palette_semantic_names() {
  CHECK_EQ_INT(kTextPaletteCount, 13);
  CHECK_STR_EQ(text_palette_name(0), "black");
  CHECK_STR_EQ(text_palette_name(12), "heritage_green");
  CHECK(text_palette_name(13) == nullptr);
  uint8_t idx = 0;
  CHECK(text_palette_index_from_name("heritage_green", &idx));
  CHECK_EQ_INT(idx, 12);
  CHECK(!text_palette_index_from_name("neon", &idx));
  // Heritage default color equals the accepted LED-L007 text color exactly.
  const Rgb heritage = text_palette_color(kDefaultTextFg);
  const Rgb expected = hsv2rgb(96, 200, 255);
  CHECK(heritage == expected);
  // Out-of-range reads black (fail closed).
  CHECK(text_palette_color(200) == kRgbBlack);
}

void test_text_readable_foreground() {
  const Rgb black{0, 0, 0};
  const Rgb white{255, 255, 255};
  const Rgb yellow{255, 200, 0};
  const Rgb blue{0, 60, 255};
  CHECK(readable_foreground(black) == white);
  CHECK(readable_foreground(white) == black);
  CHECK(readable_foreground(yellow) == black);  // yellow bg -> black glyphs
  CHECK(readable_foreground(blue) == white);    // blue bg -> white glyphs
}

void test_text_codepoint_width_and_skip() {
  // ASCII: identical to the heritage byte model.
  CHECK_EQ_INT(count_codepoints("AB12"), 4);
  CHECK_EQ_INT(string_width_px("AB12"), 24);
  CHECK_EQ_INT(count_skipped_characters("AB12"), 0);
  // Albanian letters are one code point each (2 UTF-8 bytes) and supported.
  const char* albanian = "QENE \xC3\x8B""SHT\xC3\x8B";  // "QENE ËSHTË"
  CHECK_EQ_INT(count_codepoints(albanian), 10);
  CHECK_EQ_INT(string_width_px(albanian), 60);
  CHECK_EQ_INT(count_skipped_characters(albanian), 0);
  const char*alc = "\xC3\x87\xC3\xA7";  // Çç
  CHECK_EQ_INT(count_codepoints(alc), 2);
  CHECK_EQ_INT(count_skipped_characters(alc), 0);
  // Unsupported code points render as space and count ONCE each.
  CHECK_EQ_INT(count_skipped_characters("a~b"), 1);      // '~' unsupported
  CHECK_EQ_INT(count_skipped_characters("\xE2\x82\xAC"), 1);  // € unsupported
  // Invalid UTF-8: each invalid byte counts once (renders as space).
  CHECK_EQ_INT(count_skipped_characters("\xC3"), 1);          // truncated pair
  CHECK_EQ_INT(count_skipped_characters("\xFF"), 1);          // stray continuation
  CHECK_EQ_INT(count_skipped_characters("\xC3\x41"), 1);      // invalid lead consumed alone; 'A' is valid
}

void test_text_lowercase_and_punctuation_supported() {
  for (char c = 'a'; c <= 'z'; ++c) {
    CHECK(ledwall::text::glyph_supported(c));
  }
  const char* punct = ",?:;'\"()+=%/*#_<>-.!";
  for (const char* p = punct; *p; ++p) {
    CHECK(ledwall::text::glyph_supported(*p));
  }
  // Still unsupported (deterministic, safe).
  CHECK(!ledwall::text::glyph_supported_cp(0x20AC));  // €
  CHECK(!ledwall::text::glyph_supported_cp(0x1F600)); // emoji
}

namespace {
int lit_on_side(const FrameBuffer& f, bool left_half) {
  int count = 0;
  for (uint8_t y = 0; y < 10; ++y) {
    for (uint8_t x = 0; x < 30; ++x) {
      if ((left_half ? x < 15 : x >= 15) && f.at(x, y) != kRgbBlack) {
        ++count;
      }
    }
  }
  return count;
}
}  // namespace

void test_text_rtl_marquee_enters_from_right() {
  FrameBuffer f;
  const char* txt = "ABC";
  // t=0: fully off the RIGHT edge (invisible).
  draw_scrolling_rtl(f, txt, Rgb{255, 255, 255}, 0, 100);
  CHECK(lit_on_side(f, true) + lit_on_side(f, false) == 0);
  // Entering: offset 9 px → the head is on the RIGHT half (x0 = 21).
  draw_scrolling_rtl(f, txt, Rgb{255, 255, 255}, 900, 100);
  CHECK(lit_on_side(f, false) > 0);
  // Heritage LTR enters tail-first from the LEFT: offset 9 → x0 = -9, body on
  // the left half.
  FrameBuffer g;
  draw_scrolling(g, txt, Rgb{255, 255, 255}, 900, 100);
  CHECK(lit_on_side(g, true) > 0);
  CHECK(lit_on_side(g, false) == 0);
}

void test_text_rtl_full_traversal_and_wrap() {
  const char* txt = "UNE";
  const uint32_t span = scroll_span_px(txt);  // 18 + 30 = 48
  CHECK_EQ_INT(span, 48);
  FrameBuffer f;
  draw_scrolling_rtl(f, txt, Rgb{255, 255, 255}, 100 * span, 100);  // one full wrap
  bool any = false;
  for (uint16_t i = 0; i < kLedCount; ++i) any = any || !(f.data()[i] == kRgbBlack);
  CHECK(!any);  // wrapped back to the start (invisible)
}

void test_text_word_by_word_sequence() {
  TextProgram p;
  snprintf(p.current, sizeof(p.current), "%s", "UNE JAM REDI");
  p.mode = TextMode::Words;
  p.word_dwell_ms = 2000;
  p.loop = false;
  p.fg_color = 1;  // white for exact frame checks
  FrameBuffer f;
  // Word 0 "UNE": width 18, x0=6; N's stem col0 lands at (12,2) (anchor y=2,
  // bit0 = top row). Verified against the rendered frame.
  draw_text_program(f, p, 0);
  CHECK(f.at(12, 2) != kRgbBlack);
  CHECK(f.at(2, 2) == kRgbBlack);    // left of the word is blank
  // Word 1 "JAM": J's stem at (10,2).
  draw_text_program(f, p, 2000);
  CHECK(f.at(10, 2) != kRgbBlack);
  // Word 2 "REDI": R's stem at (3,2).
  draw_text_program(f, p, 4000);
  CHECK(f.at(3, 2) != kRgbBlack);
  // One-shot completion: after the last dwell the display is a stable blank.
  draw_text_program(f, p, 6000);
  bool any = false;
  for (uint16_t i = 0; i < kLedCount; ++i) any = any || !(f.data()[i] == kRgbBlack);
  CHECK(!any);
  // Loop mode wraps back to the first word.
  p.loop = true;
  draw_text_program(f, p, 6000);
  CHECK(f.at(12, 2) != kRgbBlack);
  CHECK_EQ_INT(count_words("UNE JAM REDI"), 3);
  CHECK_EQ_INT(count_words("  double   spaces  "), 2);
  CHECK_EQ_INT(count_words(""), 0);
  CHECK_EQ_INT(words_total_ms(p), 6000);
}

void test_text_loop_marquee_never_freezes() {
  // A long message must still be moving after many full wraps (no freezing
  // after one pass): frame(t) == frame(t + k*span).
  TextProgram p;
  snprintf(p.current, sizeof(p.current), "%s", "THE LED WALL PREMIUM EXPERIENCE");
  p.mode = TextMode::Marquee;
  p.rtl = true;
  p.speed_ms_per_px = 60;
  const uint32_t span = scroll_span_px(p.current);
  const uint32_t wrap_ms = span * p.speed_ms_per_px;  // exact wrap period
  FrameBuffer a, b;
  draw_text_program(a, p, 123456);
  draw_text_program(b, p, 123456 + 7 * wrap_ms);
  bool same = true;
  for (uint16_t i = 0; i < kLedCount; ++i) same = same && (a.data()[i] == b.data()[i]);
  CHECK(same);
}

void test_text_styles_render() {
  const Rgb kWhiteRef{255, 255, 255};
  const Rgb kBlackRef{0, 0, 0};
  TextProgram p;
  snprintf(p.current, sizeof(p.current), "%s", "HI");
  p.mode = TextMode::Fixed;
  // "HI" geometry (probe-verified): H stem col0 at x=9 (all rows), I cap at
  // x=16/x=18; the H crossbar sits at y=5.
  // Solid: black background + chosen fg.
  p.style = TextStyle::Solid;
  p.fg_color = 3;  // red
  p.bg_color = 0;
  FrameBuffer f;
  draw_text_program(f, p, 0);
  CHECK(f.at(9, 4) == text_palette_color(3));  // glyph pixel in the chosen fg
  CHECK(f.at(0, 0) == kBlackRef);              // black background
  // ColorBg: user bg + readable fg.
  p.style = TextStyle::ColorBg;
  p.bg_color = 1;  // white bg
  FrameBuffer g;
  draw_text_program(g, p, 0);
  CHECK(g.at(0, 0) == kWhiteRef);  // white background
  CHECK(g.at(9, 4) == kBlackRef);  // readable black glyphs
  // RainbowText: hue varies along the glyph pixels (deterministic, lit).
  p.style = TextStyle::RainbowText;
  p.bg_color = 0;
  FrameBuffer h;
  draw_text_program(h, p, 0);
  CHECK(h.at(9, 4) != kRgbBlack);
  CHECK(h.at(0, 0) == kRgbBlack);  // rainbow text keeps a black background
  // RainbowBg: cycling background + white glyphs.
  p.style = TextStyle::RainbowBg;
  FrameBuffer i2;
  draw_text_program(i2, p, 0);
  CHECK(i2.at(0, 0) != kRgbBlack);   // colored bg everywhere
  CHECK(i2.at(9, 4) == kWhiteRef);   // white glyphs
}

void test_text_studio_program_complete_semantics() {
  TextProgram p;
  snprintf(p.current, sizeof(p.current), "%s", "HELLO THERE");
  p.started_ms = 1000;
  // Fixed: complete when actually shown.
  p.mode = TextMode::Fixed;
  CHECK(ledwall::renderer::text_program_complete(p, 1000, true));
  CHECK(!ledwall::renderer::text_program_complete(p, 1000, false));
  // Marquee: one full traversal (rtl or ltr — same span).
  p.mode = TextMode::Marquee;
  p.rtl = true;
  p.speed_ms_per_px = 50;
  const uint32_t cycle = 50 * scroll_span_px(p.current);
  CHECK(!ledwall::renderer::text_program_complete(p, 1000 + cycle - 1, true));
  CHECK(ledwall::renderer::text_program_complete(p, 1000 + cycle, true));
  // Words one-shot: last word's dwell elapsed.
  snprintf(p.current, sizeof(p.current), "%s", "A B C");
  p.mode = TextMode::Words;
  p.loop = false;
  p.word_dwell_ms = 500;
  CHECK(!ledwall::renderer::text_program_complete(p, 1000 + 1499, true));
  CHECK(ledwall::renderer::text_program_complete(p, 1000 + 1500, true));
  // Looping words never complete (continuous repetition).
  p.loop = true;
  CHECK(!ledwall::renderer::text_program_complete(p, 1000 + 100000, true));
}
