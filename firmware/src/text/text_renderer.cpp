#include "text/text_renderer.h"

#include <cstring>

#include "render/color.h"
#include "render/matrix_mapping.h"
#include "text/font5x7.h"
#include "text/text_style.h"

namespace ledwall {
namespace text {

namespace {

constexpr int16_t kCanvasW = static_cast<int16_t>(kMatrixWidth);
constexpr int8_t kTextAnchorY = 2;  // heritage demoScroll draw anchor

// Paint context: resolves each pixel's color by semantic style — the only
// pixel-level style code in the firmware. Flat styles use fg/bg directly;
// rainbow styles synthesize per-column hues (stable on the absolute screen
// column while scrolling, drifting slowly with time).
struct PaintCtx {
  TextStyle style{TextStyle::Solid};
  Rgb fg{0, 0, 0};
  Rgb bg{0, 0, 0};
  uint32_t elapsed{0};
};

Rgb pixel_color(const PaintCtx& ctx, int16_t px, bool on) {
  switch (ctx.style) {
    case TextStyle::Solid:
      return on ? ctx.fg : kRgbBlack;
    case TextStyle::ColorBg:
      return on ? ctx.fg : ctx.bg;
    case TextStyle::RainbowText:
      if (!on) {
        return kRgbBlack;
      }
      return hsv2rgb(static_cast<uint8_t>(px * 8u + (ctx.elapsed / 4u) % 256u), 255, 255);
    case TextStyle::RainbowBg:
      if (on) {
        return Rgb{255, 255, 255};
      }
      return hsv2rgb(static_cast<uint8_t>(px * 4u + (ctx.elapsed / 8u) % 256u), 255, 90);
  }
  return kRgbBlack;
}

PaintCtx make_ctx(const TextProgram& program, uint32_t elapsed_ms) {
  PaintCtx ctx;
  ctx.style = program.style;
  ctx.fg = text_palette_color(program.fg_color);
  ctx.bg = text_palette_color(program.bg_color);
  if (program.style == TextStyle::ColorBg) {
    ctx.fg = readable_foreground(ctx.bg);  // readability beats the chosen fg
  }
  ctx.elapsed = elapsed_ms;
  return ctx;
}

void paint_background(FrameBuffer& frame, const PaintCtx& ctx) {
  if (ctx.style == TextStyle::Solid || ctx.style == TextStyle::RainbowText) {
    return;  // black background (frame.clear() by the caller)
  }
  for (int16_t y = 0; y < static_cast<int16_t>(kMatrixHeight); ++y) {
    for (int16_t x = 0; x < kCanvasW; ++x) {
      frame.set(x, y, pixel_color(ctx, x, false));
    }
  }
}

// Heritage drawChar(): one glyph at (x0, y0), column bytes with bit0 = top
// row, clipped at the canvas (FrameBuffer::set drops out-of-bounds writes).
// UTF-8 aware: `c` may start a multi-byte sequence; `bytes` receives the
// consumed length for the caller's cursor advance.
void draw_char(FrameBuffer& frame, int16_t x0, int8_t y0, const char* c, size_t* bytes,
               const PaintCtx& ctx) {
  uint32_t cp = 0;
  *bytes = utf8_decode(c, &cp);
  if (*bytes == 0) {
    return;
  }
  uint8_t cols[kGlyphWidth];
  glyph_cols_cp(cp, cols);
  for (uint8_t col = 0; col < kGlyphWidth; ++col) {
    for (uint8_t row = 0; row < kGlyphHeight; ++row) {
      if (cols[col] & (1u << row)) {
        frame.set(static_cast<int16_t>(x0 + col), static_cast<int16_t>(y0 + row),
                  pixel_color(ctx, static_cast<int16_t>(x0 + col), true));
      }
    }
  }
}

// Heritage drawString(): 6 px advance per character (per code point).
void draw_string(FrameBuffer& frame, int16_t x0, int8_t y0, const char* text,
                 const PaintCtx& ctx) {
  int16_t x = x0;
  size_t bytes = 0;
  for (const char* p = text; *p != '\0'; p += bytes) {
    draw_char(frame, x, y0, p, &bytes, ctx);
    x += kAdvancePx;
  }
}

void draw_marquee(FrameBuffer& frame, const char* text, const PaintCtx& ctx, int16_t x0) {
  // V-L009-1 (LED-L009 repair cycle 2, root cause): the marquee path is the
  // only text path that did NOT clear the framebuffer first. The legacy
  // draw_scrolling() callers cleared, so LED-L007-era scroll:true scrolled
  // cleanly — but the LED-L009 Text Studio marquee (draw_text_program → here,
  // solid/rainbow_text styles) re-rendered every frame ON TOP of the previous
  // one: on the panel the moving text smeared into a continuously illuminated
  // rectangular block spanning the whole scroll path. Clear here, at the
  // single choke point every marquee caller shares; background styles repaint
  // the canvas right after, so their appearance is unchanged.
  frame.clear();
  paint_background(frame, ctx);
  draw_string(frame, x0, kTextAnchorY, text, ctx);
}

void draw_centered(FrameBuffer& frame, const char* text, const PaintCtx& ctx) {
  const int16_t width = static_cast<int16_t>(string_width_px(text));
  int16_t x0 = 0;
  if (width < kCanvasW) {
    x0 = static_cast<int16_t>((kCanvasW - width) / 2);
  }
  paint_background(frame, ctx);
  draw_string(frame, x0, kTextAnchorY, text, ctx);
}

// Copies word `index` (0-based) into out (cap bytes, NUL-terminated).
void extract_word(const char* text, uint16_t index, char* out, size_t cap) {
  out[0] = '\0';
  if (text == nullptr || cap == 0) {
    return;
  }
  uint16_t current = 0;
  bool in_word = false;
  size_t used = 0;
  size_t bytes = 0;
  for (const char* p = text; *p != '\0'; p += bytes) {
    uint32_t cp = 0;
    bytes = utf8_decode(p, &cp);
    if (bytes == 0) {
      break;
    }
    const bool space = (cp == ' ');
    if (!space) {
      if (current == index && used + bytes < cap) {
        for (size_t i = 0; i < bytes; ++i) {
          out[used + i] = p[i];
        }
        used += bytes;
        out[used] = '\0';
      }
      in_word = true;
    } else if (in_word) {
      if (current == index) {
        return;  // word complete
      }
      ++current;
      in_word = false;
      used = 0;
      out[0] = '\0';
    }
  }
}

}  // namespace

size_t count_codepoints(const char* text) {
  if (text == nullptr) {
    return 0;
  }
  size_t count = 0;
  size_t bytes = 0;
  for (const char* p = text; *p != '\0'; p += bytes) {
    uint32_t cp = 0;
    bytes = utf8_decode(p, &cp);
    if (bytes == 0) {
      break;
    }
    ++count;
  }
  return count;
}

size_t string_width_px(const char* text) { return count_codepoints(text) * kAdvancePx; }

uint16_t count_skipped_characters(const char* text) {
  if (text == nullptr) {
    return 0;
  }
  uint16_t skipped = 0;
  size_t bytes = 0;
  for (const char* p = text; *p != '\0'; p += bytes) {
    uint32_t cp = 0;
    bytes = utf8_decode(p, &cp);
    if (bytes == 0) {
      break;
    }
    if (!glyph_supported_cp(cp)) {
      // Invalid single bytes and unsupported code points each count once
      // (each renders as one 6 px-wide space).
      ++skipped;
    }
  }
  return skipped;
}

bool fits_static(const char* text) { return string_width_px(text) <= kCanvasW; }

void draw_static(FrameBuffer& frame, const char* text, Rgb color) {
  frame.clear();
  PaintCtx ctx;  // Solid with the given color
  ctx.fg = color;
  draw_centered(frame, text, ctx);
}

void draw_scrolling(FrameBuffer& frame, const char* text, Rgb color, uint32_t elapsed_ms,
                    uint32_t speed_ms_per_px) {
  frame.clear();
  if (speed_ms_per_px == 0) {
    speed_ms_per_px = 1;  // defensive: division guard (dispatcher clamps to 40..500)
  }
  PaintCtx ctx;
  ctx.fg = color;
  const int16_t width = static_cast<int16_t>(string_width_px(text));
  const int16_t span = static_cast<int16_t>(width + kCanvasW);
  // Heritage demoScroll model: the offset grows left→right, the text enters
  // tail-first at the left edge, traverses, exits right, and wraps.
  const int16_t offset =
      static_cast<int16_t>((elapsed_ms / speed_ms_per_px) % static_cast<uint32_t>(span)) - width;
  draw_marquee(frame, text, ctx, offset);
}

void draw_scrolling_rtl(FrameBuffer& frame, const char* text, Rgb color, uint32_t elapsed_ms,
                        uint32_t speed_ms_per_px) {
  frame.clear();
  if (speed_ms_per_px == 0) {
    speed_ms_per_px = 1;
  }
  PaintCtx ctx;
  ctx.fg = color;
  const int16_t width = static_cast<int16_t>(string_width_px(text));
  const int16_t span = static_cast<int16_t>(width + kCanvasW);
  // Normal reading direction: the text enters from the RIGHT edge at t=0
  // (x0 = +30, fully off-canvas) and moves naturally toward the left,
  // exiting fully at offset = span (x0 = -width), then wraps.
  const int16_t offset_px =
      static_cast<int16_t>((elapsed_ms / speed_ms_per_px) % static_cast<uint32_t>(span));
  const int16_t x0 = static_cast<int16_t>(kCanvasW - offset_px);
  draw_marquee(frame, text, ctx, x0);
}

uint32_t scroll_span_px(const char* text) {
  return static_cast<uint32_t>(string_width_px(text) + kCanvasW);
}

uint16_t count_words(const char* text) {
  if (text == nullptr) {
    return 0;
  }
  uint16_t count = 0;
  bool in_word = false;
  size_t bytes = 0;
  for (const char* p = text; *p != '\0'; p += bytes) {
    uint32_t cp = 0;
    bytes = utf8_decode(p, &cp);
    if (bytes == 0) {
      break;
    }
    const bool space = (cp == ' ');
    if (!space && !in_word) {
      ++count;
      in_word = true;
    } else if (space) {
      in_word = false;
    }
  }
  return count;
}

uint32_t words_total_ms(const TextProgram& program) {
  return static_cast<uint32_t>(count_words(program.current)) * program.word_dwell_ms;
}

void draw_text_program(FrameBuffer& frame, const TextProgram& program, uint32_t elapsed_ms) {
  const PaintCtx ctx = make_ctx(program, elapsed_ms);

  // Legacy projection (renderer.h parity): mode Fixed + scrolling flag is the
  // heritage LEFT→RIGHT marquee, whatever the (newer) rtl field says.
  const TextMode mode =
      (program.mode == TextMode::Fixed && program.scrolling) ? TextMode::Marquee : program.mode;
  const bool rtl = (mode == TextMode::Marquee && program.mode == TextMode::Fixed)
                       ? false
                       : program.rtl;

  switch (mode) {
    case TextMode::Fixed:
      frame.clear();
      draw_centered(frame, program.current, ctx);
      return;

    case TextMode::Marquee: {
      uint32_t speed = program.speed_ms_per_px;
      if (speed == 0) {
        speed = 1;  // defensive (dispatcher clamps 40..500)
      }
      const int16_t width = static_cast<int16_t>(string_width_px(program.current));
      const int16_t span = static_cast<int16_t>(width + kCanvasW);
      const int16_t offset_px =
          static_cast<int16_t>((elapsed_ms / speed) % static_cast<uint32_t>(span));
      const int16_t x0 = rtl
                             ? static_cast<int16_t>(kCanvasW - offset_px)  // RIGHT→LEFT (default)
                             : static_cast<int16_t>(offset_px - width);    // heritage LEFT→RIGHT
      draw_marquee(frame, program.current, ctx, x0);
      return;
    }

    case TextMode::Words: {
      const uint16_t words = count_words(program.current);
      frame.clear();
      if (words == 0) {
        return;
      }
      const uint32_t dwell =
          program.word_dwell_ms == 0 ? kWordDwellDefaultMs : program.word_dwell_ms;
      const uint32_t word_index_raw = elapsed_ms / dwell;
      if (!program.loop && word_index_raw >= words) {
        return;  // one-shot completed: stable blank (text.done fired)
      }
      const uint16_t word_index = static_cast<uint16_t>(word_index_raw % words);  // loop wraps
      char word[kTextMaxLen];
      extract_word(program.current, word_index, word, sizeof(word));
      draw_centered(frame, word, ctx);
      return;
    }
  }
}

}  // namespace text
}  // namespace ledwall
