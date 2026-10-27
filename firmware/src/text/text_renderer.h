#pragma once

#include <cstddef>
#include <cstdint>

#include "api/command.h"  // TextProgram (§4 text program model)
#include "render/framebuffer.h"
#include "render/pixel.h"

namespace ledwall {
namespace text {

// Text pipeline (frozen spec §6 contract over the INHERITED heritage
// renderer, extended by the LED-L009 Text Studio). Pure functions — no
// command parsing, no device state.
//
// LED-L009 semantics (documented in CONTRACT.md):
// - Width/skip counting is UTF-8 code-point based (the LED-L007 byte-based
//   counting was a documented accepted observation O-L007-R5; the extended
//   charset with multi-byte Albanian letters makes code points the truthful
//   unit). Unsupported code points render as space and are counted in
//   skippedCharacters.
// - Modes: Fixed (static), Marquee (continuous wrap; RTL = the normal reading
//   direction and the new default, LTR = the preserved heritage direction),
//   Words (word-by-word with configurable dwell, loop or one-shot).
// - Styles: Solid / ColorBg / RainbowText / RainbowBg (semantic, see
//   text_style.h).

// Rendered width in px: code-point count × 6 px advance (heritage stringWidth
// generalized to decoded code points).
size_t string_width_px(const char* text);

// Number of code points in the string.
size_t count_codepoints(const char* text);

// Number of decoded code points outside the supported glyph set (plus one
// per invalid UTF-8 byte). Rendered as space, reported in skippedCharacters.
uint16_t count_skipped_characters(const char* text);

// True iff the text renders statically within the canvas under the §6 rule:
// width ≤ 30 px (≤5 glyphs at the 6 px heritage advance).
bool fits_static(const char* text);

// Static rendering, centered horizontally, heritage vertical anchor y=2.
// Legacy flat-color form (Solid style with the given color).
void draw_static(FrameBuffer& frame, const char* text, Rgb color);

// Heritage scrolling model (LEFT→RIGHT: the text enters tail-first at the
// left edge, traverses, exits right, wraps). Preserved unchanged as the
// legacy/reverse direction; the product default is draw_text_program with
// rtl=true. elapsed_ms is program-relative and wrap-safe.
void draw_scrolling(FrameBuffer& frame, const char* text, Rgb color,
                    uint32_t elapsed_ms, uint32_t speed_ms_per_px);

// RIGHT→LEFT marquee (the normal reading direction): the text enters from
// the right edge and moves naturally toward the left, wrapping.
void draw_scrolling_rtl(FrameBuffer& frame, const char* text, Rgb color,
                        uint32_t elapsed_ms, uint32_t speed_ms_per_px);

// Total scroll traversal span in px (width + canvas width; heritage span).
uint32_t scroll_span_px(const char* text);

// Number of whitespace-separated words (bounded scan; empty text → 0).
uint16_t count_words(const char* text);

// Total one-shot word-by-word duration (word count × dwell).
uint32_t words_total_ms(const TextProgram& program);

// Full Text Studio renderer: realizes mode (fixed / marquee / words) and
// style (solid / color_bg / rainbow_text / rainbow_bg) for the given
// program-relative elapsed time. Deterministic; clears the frame first.
void draw_text_program(FrameBuffer& frame, const TextProgram& program, uint32_t elapsed_ms);

}  // namespace text
}  // namespace ledwall
