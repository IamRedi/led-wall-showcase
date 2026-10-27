#pragma once

#include <cstddef>
#include <cstdint>

#include "render/pixel.h"

namespace ledwall {
namespace text {

// LED-L009 Text Studio — semantic text presentation model. Styles and colors
// are named semantic values (never renderer internals); the API exposes only
// these names, and the render layer realizes them. Clients (dashboard, HA,
// HUGO) can therefore request "rainbow_text on black" without any pixel
// knowledge (frozen spec §3/§16 boundary).

// Semantic text styles (wire names are the exact strings).
enum class TextStyle : uint8_t {
  Solid = 0,      // black background + user text color
  ColorBg,        // user background + automatically readable foreground
  RainbowText,    // black background + per-column rainbow glyph pixels
  RainbowBg,      // cycling rainbow background + white glyphs
};
constexpr uint8_t kTextStyleCount = 4;
constexpr const char* const kTextStyleNames[kTextStyleCount] = {
    "solid", "color_bg", "rainbow_text", "rainbow_bg",
};

const char* text_style_name(TextStyle style);
bool text_style_from_name(const char* name, TextStyle* out);

// Semantic text palette (LED-L009, wire names are the exact strings). The
// RGB values are the documented realization of each name (deterministic).
// Index 12 preserves the accepted heritage text color CHSV(96,200,255)
// through the deterministic hsv2rgb converter — the LED-L007 default look.
struct PaletteEntry {
  const char* name;
  Rgb color;
};

constexpr uint8_t kTextPaletteCount = 13;
constexpr PaletteEntry kTextPalette[kTextPaletteCount] = {
    {"black", {0, 0, 0}},
    {"white", {255, 255, 255}},
    {"gray", {110, 110, 110}},
    {"red", {255, 0, 0}},
    {"orange", {255, 90, 0}},
    {"yellow", {255, 200, 0}},
    {"green", {0, 160, 40}},
    {"cyan", {0, 200, 200}},
    {"blue", {0, 60, 255}},
    {"purple", {150, 0, 255}},
    {"pink", {255, 60, 140}},
    {"warm_white", {255, 140, 60}},
    {"heritage_green", {55, 255, 102}},  // hsv2rgb(96, 200, 255)
};

constexpr uint8_t kDefaultTextFg = 12;  // heritage_green (LED-L007 default look)
constexpr uint8_t kDefaultTextBg = 0;   // black

const char* text_palette_name(uint8_t index);            // nullptr if out of range
bool text_palette_index_from_name(const char* name, uint8_t* out);
Rgb text_palette_color(uint8_t index);                   // black if out of range

// Readable foreground for ColorBg: white or black by perceived luminance.
Rgb readable_foreground(Rgb background);

}  // namespace text
}  // namespace ledwall
