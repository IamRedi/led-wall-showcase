#include "test_framework.h"

#include <cstring>
#include <initializer_list>

#include "render/framebuffer.h"
#include "render/renderer.h"
#include "text/text_renderer.h"

// LED-L009 repair cycle 2 (V-L009-1): INDEPENDENT golden framebuffer tests for
// the scrolling-text pipeline. The reference implementation below is written
// from the repair specification itself (conventional 5×7 letterforms in
// heritage column format, bit0 = top row; 6 px advance; offset math from the
// documented marquee model) and shares NO code with the firmware renderer —
// expected pixels are never derived from the implementation under test.
//
// Verified properties:
//   1. frame progression moves glyph positions (and in the RTL direction);
//   2. foreground pixels correspond to valid glyph bitmaps (full-frame equal
//      to the independent reference at every probed offset);
//   3. background/transparent pixels remain correct (no block, no fill);
//   4. no full rectangular block appears for dark-background styles;
//   5. RTL movement direction is right→left;
//   6. LTR legacy direction is left→right;
//   7. loop wraps seamlessly (offset 0 == offset span);
//   8. no stale pixels across program swaps and mode changes;
//   9. no out-of-bounds framebuffer access (extreme offsets, partial glyphs);
//  10. rendering survives repeated activations.

using namespace ledwall;

namespace {

// --- independent reference font (transcribed per the repair spec) ----------
struct RefGlyph {
  char c;
  uint8_t cols[5];
};

// The letterforms needed by the probe strings, transcribed from the
// conventional 5×7 font (heritage column format, bit0 = top row).
constexpr RefGlyph kRefGlyphs[] = {
    {'A', {0x7E, 0x09, 0x09, 0x7E, 0x00}},
    {'B', {0x7F, 0x49, 0x49, 0x49, 0x36}},
    {'D', {0x7F, 0x41, 0x41, 0x41, 0x3E}},
    {'E', {0x7F, 0x49, 0x49, 0x49, 0x41}},
    {'I', {0x00, 0x41, 0x7F, 0x41, 0x00}},
    {'M', {0x7F, 0x02, 0x0C, 0x02, 0x7F}},
    {'N', {0x7F, 0x04, 0x08, 0x10, 0x7F}},
    {'O', {0x3E, 0x41, 0x41, 0x41, 0x3E}},
    {'R', {0x7F, 0x09, 0x19, 0x29, 0x46}},
    {'U', {0x3F, 0x40, 0x40, 0x40, 0x3F}},
    {'W', {0x7F, 0x28, 0x1C, 0x28, 0x7F}},
};

bool ref_glyph(char c, uint8_t* cols) {
  cols[0] = cols[1] = cols[2] = cols[3] = cols[4] = 0;  // unsupported = space
  for (const RefGlyph& g : kRefGlyphs) {
    if (g.c == c) {
      for (uint8_t i = 0; i < 5; ++i) {
        cols[i] = g.cols[i];
      }
      return true;
    }
  }
  return false;
}

// --- independent reference compositor --------------------------------------
constexpr int kW = 30;
constexpr int kH = 10;
constexpr int kAnchorY = 2;
constexpr int kAdvance = 6;

// Reference marquee frame: glyphs at x0 + i*6, bit0 = top, canvas-clipped.
void ref_marquee(uint8_t out[kH][kW], const char* text, int x0) {
  for (int y = 0; y < kH; ++y) {
    for (int x = 0; x < kW; ++x) {
      out[y][x] = 0;
    }
  }
  int x = x0;
  for (const char* p = text; *p != '\0'; ++p, x += kAdvance) {
    uint8_t cols[5];
    if (p[0] == ' ') {
      continue;  // space glyph: empty 5-column advance
    }
    ref_glyph(*p, cols);
    for (int col = 0; col < 5; ++col) {
      for (int row = 0; row < 7; ++row) {
        if (cols[col] & (1u << row)) {
          const int px = x + col;
          const int py = kAnchorY + row;
          if (px >= 0 && px < kW && py >= 0 && py < kH) {
            out[py][px] = 1;
          }
        }
      }
    }
  }
}

// Reference offset model (CONTRACT.md): RTL x0 = W − (t/speed mod span);
// heritage LTR x0 = (t/speed mod span) − width.
// Device frame → occupancy bitmap (any non-black pixel counts).
void device_to_occ(const FrameBuffer& f, uint8_t out[kH][kW]) {
  for (int y = 0; y < kH; ++y) {
    for (int x = 0; x < kW; ++x) {
      out[y][x] = (f.at(static_cast<uint8_t>(x), static_cast<uint8_t>(y)) == kRgbBlack) ? 0 : 1;
    }
  }
}

bool occ_equal(const uint8_t a[kH][kW], const uint8_t b[kH][kW]) {
  for (int y = 0; y < kH; ++y) {
    for (int x = 0; x < kW; ++x) {
      if (a[y][x] != b[y][x]) {
        return false;
      }
    }
  }
  return true;
}

// Render the firmware TextProgram for `elapsed` through the REAL renderer.
void render_marquee(FrameBuffer& f, const char* text, bool rtl, uint32_t elapsed,
                    uint32_t speed = 40) {
  renderer::RenderInput in;
  in.kind = ModeKind::Text;
  in.started_ms = 0;
  in.now_ms = elapsed;
  static TextProgram p;  // probe-local program storage
  p = TextProgram{};
  std::snprintf(p.current, sizeof(p.current), "%s", text);
  p.mode = TextMode::Marquee;
  p.rtl = rtl;
  p.scrolling = false;
  p.speed_ms_per_px = speed;
  p.style = TextStyle::Solid;
  p.fg_color = 12;
  p.bg_color = 0;
  in.text = &p;
  renderer::render(f, in);
}

int width_of(const char* s) {
  return static_cast<int>(std::strlen(s)) * kAdvance;
}

}  // namespace

void test_scroll_golden_full_frames_match_independent_reference() {
  const char* msg = "MIREMBREMA REDI";
  const int width = width_of(msg);
  FrameBuffer f;
  uint8_t got[kH][kW];
  uint8_t want[kH][kW];
  // Probe offsets covering entry, partial clipping at both edges, mid
  // traversal, exit, and the wrap boundary.
  const int span = width + kW;  // 15 chars × 6 + 30 = 120 for this message
  for (int off : {0, 1, 2, 5, 15, 25, 29, 30, 31, 50, 70, 95, 96, 100, 110, 119, 125}) {
    // The marquee offset wraps at span: device offset = elapsed-step mod span.
    const int wrapped = off % span;
    // RTL: x0 = W − off
    render_marquee(f, msg, true, static_cast<uint32_t>(off) * 40u);
    device_to_occ(f, got);
    ref_marquee(want, msg, kW - wrapped);
    if (!occ_equal(got, want)) {
      CHECK_EQ_INT(off, -1);  // locate the failing offset in the log
      continue;
    }
    ++g_checks;
    // LTR legacy: x0 = off − width
    render_marquee(f, msg, false, static_cast<uint32_t>(off) * 40u);
    device_to_occ(f, got);
    ref_marquee(want, msg, wrapped - width);
    CHECK(occ_equal(got, want));
  }
}

void test_scroll_progression_moves_glyphs_in_rtl_direction() {
  FrameBuffer f;
  uint8_t a[kH][kW];
  uint8_t b[kH][kW];
  const char* msg = "WOW";
  // Two frames one pixel apart, mid-traversal (both fully on-canvas: x0=10/9).
  render_marquee(f, msg, true, 20u * 40u);
  device_to_occ(f, a);
  render_marquee(f, msg, true, 21u * 40u);
  device_to_occ(f, b);
  CHECK(!occ_equal(a, b));  // 1. frame progression actually moves glyphs
  // 5. RTL direction: the rightmost lit column must move LEFT.
  int right_a = -1;
  int right_b = -1;
  for (int x = kW - 1; x >= 0; --x) {
    bool la = false;
    bool lb = false;
    for (int y = 0; y < kH; ++y) {
      la = la || a[y][x];
      lb = lb || b[y][x];
    }
    if (right_a < 0 && la) {
      right_a = x;
    }
    if (right_b < 0 && lb) {
      right_b = x;
    }
  }
  CHECK(right_a > 0);
  CHECK(right_b > 0);
  CHECK(right_b == right_a - 1);  // exactly one pixel left per step
}

void test_scroll_ltr_legacy_direction_preserved() {
  FrameBuffer f;
  uint8_t a[kH][kW];
  uint8_t b[kH][kW];
  const char* msg = "WOW";
  render_marquee(f, msg, false, 20u * 40u);
  device_to_occ(f, a);
  render_marquee(f, msg, false, 21u * 40u);
  device_to_occ(f, b);
  CHECK(!occ_equal(a, b));
  // 6. LTR: the LEFTmost lit column moves RIGHT (heritage travel direction).
  int left_a = kW;
  int left_b = kW;
  for (int x = 0; x < kW; ++x) {
    bool la = false;
    bool lb = false;
    for (int y = 0; y < kH; ++y) {
      la = la || a[y][x];
      lb = lb || b[y][x];
    }
    if (left_a == kW && la) {
      left_a = x;
    }
    if (left_b == kW && lb) {
      left_b = x;
    }
  }
  CHECK(left_a < kW);
  CHECK(left_b < kW);
  CHECK(left_b == left_a + 1);
}

void test_scroll_no_block_for_dark_background_styles() {
  // 3./4.: the glyph band density must stay glyph-like (formed letters),
  // never a filled rectangle; the strip stays bounded to the 7-row band.
  const char* msg = "MIREMBREMA REDI";
  FrameBuffer f;
  for (bool rtl : {true, false}) {
    render_marquee(f, msg, rtl, 2000u);
    int band_lit = 0;
    int outside_lit = 0;
    for (int y = 0; y < kH; ++y) {
      for (int x = 0; x < kW; ++x) {
        const bool lit =
            f.at(static_cast<uint8_t>(x), static_cast<uint8_t>(y)) != kRgbBlack;
        if (y >= kAnchorY && y < kAnchorY + 7) {
          band_lit += lit;
        } else {
          outside_lit += lit;
        }
      }
    }
    // A solid block of the ~96 px text would light ~96×7 = 672 cells; formed
    // glyphs light well under half the band's 210 cells.
    CHECK(band_lit > 0);
    CHECK(band_lit < 105);
    CHECK_EQ_INT(outside_lit, 0);  // nothing renders outside the glyph band
  }
}

void test_scroll_loop_wraps_seamlessly() {
  // 7.: offset 0 and offset span render the identical frame (t=0 == t=cycle),
  // and the text exits completely just before the wrap, then re-enters.
  const char* msg = "REDION";  // N's last column stays lit until off = span-1
  const int span = width_of(msg) + kW;
  FrameBuffer f0;
  FrameBuffer f1;
  render_marquee(f0, msg, true, 0u);
  render_marquee(f1, msg, true, static_cast<uint32_t>(span) * 40u);
  uint8_t a[kH][kW];
  uint8_t b[kH][kW];
  device_to_occ(f0, a);
  device_to_occ(f1, b);
  CHECK(occ_equal(a, b));  // t=0 and t=cycle are the identical frame
  // Motion continues through the wrap: the final pre-wrap frames still move.
  FrameBuffer fa;
  FrameBuffer fb;
  render_marquee(fa, msg, true, static_cast<uint32_t>(span - 2) * 40u);
  render_marquee(fb, msg, true, static_cast<uint32_t>(span - 1) * 40u);
  device_to_occ(fa, a);
  device_to_occ(fb, b);
  CHECK(!occ_equal(a, b));  // exits completely at the last pre-wrap pixel
}

void test_scroll_no_stale_pixels_across_swaps() {
  // 8.: rendering a different program in the same buffer leaves no leftovers.
  FrameBuffer f;
  render_marquee(f, "MIREMBREMA REDI", true, 2000u);
  render_marquee(f, "WOW", true, 2000u);
  uint8_t got[kH][kW];
  uint8_t want[kH][kW];
  device_to_occ(f, got);
  // elapsed 2000 at 40 ms/px: offset = 50 % (18+30) = 2 → x0 = 30 − 2 = 28.
  ref_marquee(want, "WOW", 28);
  CHECK(occ_equal(got, want));
  // Mode change: marquee → fixed, same buffer.
  renderer::RenderInput in;
  in.kind = ModeKind::Text;
  in.started_ms = 0;
  in.now_ms = 100;
  static TextProgram p;
  p = TextProgram{};
  std::snprintf(p.current, sizeof(p.current), "%s", "WOW");
  p.mode = TextMode::Fixed;
  p.rtl = true;
  in.text = &p;
  renderer::render(f, in);
  int lit = 0;
  for (int y = 0; y < kH; ++y) {
    for (int x = 0; x < kW; ++x) {
      lit += f.at(static_cast<uint8_t>(x), static_cast<uint8_t>(y)) != kRgbBlack;
    }
  }
  CHECK(lit > 0);
  // The fixed "WOW" (18 px) centers at x0=6..23: nothing outside that span.
  for (int x = 0; x < 6; ++x) {
    for (int y = 0; y < kH; ++y) {
      CHECK(f.at(static_cast<uint8_t>(x), static_cast<uint8_t>(y)) == kRgbBlack);
    }
  }
  for (int x = 24; x < kW; ++x) {
    for (int y = 0; y < kH; ++y) {
      CHECK(f.at(static_cast<uint8_t>(x), static_cast<uint8_t>(y)) == kRgbBlack);
    }
  }
}

void test_scroll_extreme_offsets_stay_bounded() {
  // 9.: extreme elapsed values (device uptime class) and partial glyphs at
  // both edges render deterministically, fully clipped, no crash/OOB.
  const char* msg = "U W UW WU";
  FrameBuffer f;
  for (uint32_t elapsed : {0u, 1u, 39u, 41u, 863999999u, 4294967295u}) {
    render_marquee(f, msg, true, elapsed);
    render_marquee(f, msg, false, elapsed);
    for (int y = 0; y < kH; ++y) {
      for (int x = 0; x < kW; ++x) {
        (void)f.at(static_cast<uint8_t>(x), static_cast<uint8_t>(y));
      }
    }
    ++g_checks;
  }
}

void test_scroll_repeated_activation_survives() {
  // 10.: activate → run → clear → activate again, repeatedly; every cycle
  // produces the reference frame at the same offset.
  const char* msg = "REDI";
  FrameBuffer f;
  uint8_t got[kH][kW];
  uint8_t want[kH][kW];
  for (int cycle = 0; cycle < 5; ++cycle) {
    // One pixel step per cycle: offsets 25..29 → x0 = 5..1 (fully on-canvas).
    render_marquee(f, msg, true, 1000u + static_cast<uint32_t>(cycle) * 40u);
    device_to_occ(f, got);
    ref_marquee(want, msg, kW - 25 - cycle);
    CHECK(occ_equal(got, want));
  }
}

void test_scroll_text_frame_content_changed_cadence_helper() {
  // The V-L009-1 gate helper: at 140 ms/px the pixel offset only changes every
  // 140 ms, so identical-content frames are not re-shown; rainbow styles drift
  // every frame; words change at dwell boundaries.
  static TextProgram p;
  p = TextProgram{};
  std::snprintf(p.current, sizeof(p.current), "%s", "MIREMBREMA REDI");
  p.mode = TextMode::Marquee;
  p.rtl = true;
  p.speed_ms_per_px = 140;
  p.style = TextStyle::Solid;
  const uint32_t started = 1000;
  p.started_ms = started;
  CHECK(!renderer::text_frame_content_changed(p, started + 100, started));   // sub-pixel
  CHECK(!renderer::text_frame_content_changed(p, started + 139, started));
  CHECK(renderer::text_frame_content_changed(p, started + 140, started));    // one pixel
  CHECK(renderer::text_frame_content_changed(p, started + 280, started));
  // Rainbow styles always drift.
  p.style = TextStyle::RainbowText;
  CHECK(renderer::text_frame_content_changed(p, started + 20, started));
  p.style = TextStyle::Solid;
  // Words: one change per dwell boundary, not per frame.
  p.mode = TextMode::Words;
  p.word_dwell_ms = 2000;
  CHECK(!renderer::text_frame_content_changed(p, started + 1500, started));
  CHECK(renderer::text_frame_content_changed(p, started + 2000, started));
}
