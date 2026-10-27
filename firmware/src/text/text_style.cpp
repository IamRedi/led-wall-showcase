#include "text/text_style.h"

#include <cstring>

namespace ledwall {
namespace text {

const char* text_style_name(TextStyle style) {
  if (static_cast<uint8_t>(style) < kTextStyleCount) {
    return kTextStyleNames[static_cast<uint8_t>(style)];
  }
  return nullptr;
}

bool text_style_from_name(const char* name, TextStyle* out) {
  if (name == nullptr) {
    return false;
  }
  for (uint8_t i = 0; i < kTextStyleCount; ++i) {
    if (std::strcmp(name, kTextStyleNames[i]) == 0) {
      if (out != nullptr) {
        *out = static_cast<TextStyle>(i);
      }
      return true;
    }
  }
  return false;
}

const char* text_palette_name(uint8_t index) {
  return (index < kTextPaletteCount) ? kTextPalette[index].name : nullptr;
}

bool text_palette_index_from_name(const char* name, uint8_t* out) {
  if (name == nullptr) {
    return false;
  }
  for (uint8_t i = 0; i < kTextPaletteCount; ++i) {
    if (std::strcmp(name, kTextPalette[i].name) == 0) {
      if (out != nullptr) {
        *out = i;
      }
      return true;
    }
  }
  return false;
}

Rgb text_palette_color(uint8_t index) {
  const Rgb black{0, 0, 0};
  return (index < kTextPaletteCount) ? kTextPalette[index].color : black;
}

Rgb readable_foreground(Rgb background) {
  // Perceived luminance (integer Rec.601 weights); dark backgrounds get white
  // glyphs, light backgrounds black — always readable on the panel.
  const uint32_t lum =
      299u * background.r + 587u * background.g + 114u * background.b;
  const Rgb white{255, 255, 255};
  const Rgb black{0, 0, 0};
  // Standard ~50 % perceived-luminance split: blue/colored backgrounds get
  // white glyphs; yellow/white/bright backgrounds get black glyphs.
  return lum >= 128000u ? black : white;
}

}  // namespace text
}  // namespace ledwall
