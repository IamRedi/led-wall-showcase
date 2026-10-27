#include "text/font5x7.h"

namespace ledwall {
namespace text {

namespace {

// Heritage glyph data (COPY of `PLUTO EQ ACCESSORY/src/main.cpp glyphCols`,
// byte-for-byte EXCEPT the documented corrections below). Column-major:
// 5 columns left→right, bit0 = top row. Corrections, each render-verified:
// - D-L008-4 (LED-L008 hardware campaign): 'M' was malformed and 'R' was
//   byte-identical to 'P'; corrected bytes verified on the physical panel.
// - V-L009-2 (LED-L009 repair cycle 2, Owner visual acceptance): the HERITAGE
//   source itself carries 'U' byte-identical to 'O' (legacy glyphCols case 'U'
//   = 3E 41 41 41 3E = case 'O') — the same class of heritage data corruption
//   as D-L008-4, never visible until the Owner read static letters on the
//   panel. Corrected to the conventional 5×7 U (open top, connected bottom).
struct Glyph {
  char c;
  uint8_t cols[5];
};

constexpr Glyph kHeritageGlyphs[] = {
    {'A', {0x7E, 0x09, 0x09, 0x7E, 0x00}},
    {'E', {0x7F, 0x49, 0x49, 0x49, 0x41}},
    {'H', {0x7F, 0x08, 0x08, 0x08, 0x7F}},
    {'I', {0x00, 0x41, 0x7F, 0x41, 0x00}},
    {'L', {0x7F, 0x40, 0x40, 0x40, 0x40}},
    {'M', {0x7F, 0x02, 0x0C, 0x02, 0x7F}},  // D-L008-4 corrected (was 7F 7F 49 41 41)
    {'N', {0x7F, 0x04, 0x08, 0x10, 0x7F}},
    {'O', {0x3E, 0x41, 0x41, 0x41, 0x3E}},
    {'P', {0x7F, 0x09, 0x09, 0x06, 0x00}},
    {'R', {0x7F, 0x09, 0x19, 0x29, 0x46}},  // D-L008-4 corrected (was == 'P')
    {'T', {0x00, 0x01, 0x7F, 0x01, 0x00}},
    {'U', {0x3F, 0x40, 0x40, 0x40, 0x3F}},  // V-L009-2 corrected (heritage had U == O)
    {'0', {0x3E, 0x41, 0x41, 0x41, 0x3E}},
    {'8', {0x7F, 0x49, 0x49, 0x49, 0x7F}},
    {'-', {0x00, 0x00, 0x08, 0x08, 0x00}},
};

// NEW extension glyphs (frozen §6 guaranteed set): standard 5×7 letterforms
// transposed into the heritage column format (bit0 = top row). Documented NEW
// data — not present in the heritage table. 'D' carries the D-L008-4
// correction (was 7F 41 7E 7E 7E). 'W' carries the V-L009-2 correction: the
// previous bytes (7F 20 18 20 7F — the classic shallow-apex form, an exact
// vertical mirror of the corrected M) were unambiguous in print but read as
// U-like at the panel's 5×7 scale; the apex is raised and valley strokes
// added so W reads as a double-V and stays distinguishable from V, M and U.
constexpr Glyph kExtensionGlyphs[] = {
    {'B', {0x7F, 0x49, 0x49, 0x49, 0x36}},
    {'C', {0x7E, 0x41, 0x41, 0x41, 0x63}},
    {'D', {0x7F, 0x41, 0x41, 0x41, 0x3E}},  // D-L008-4 corrected
    {'F', {0x7F, 0x09, 0x09, 0x09, 0x01}},
    {'G', {0x7E, 0x41, 0x41, 0x49, 0x79}},
    {'J', {0x20, 0x40, 0x41, 0x1F, 0x01}},
    {'K', {0x7F, 0x08, 0x14, 0x22, 0x41}},
    {'Q', {0x7E, 0x41, 0x41, 0x21, 0x5E}},
    {'S', {0x46, 0x49, 0x49, 0x39, 0x31}},
    {'V', {0x1F, 0x70, 0x40, 0x70, 0x1F}},
    {'W', {0x7F, 0x28, 0x1C, 0x28, 0x7F}},  // V-L009-2 corrected (apex raised, valleys added)
    {'X', {0x63, 0x14, 0x08, 0x14, 0x63}},
    {'Y', {0x03, 0x04, 0x78, 0x04, 0x03}},
    {'Z', {0x41, 0x51, 0x49, 0x45, 0x43}},
    {'1', {0x00, 0x42, 0x7F, 0x40, 0x00}},
    {'2', {0x42, 0x61, 0x59, 0x45, 0x47}},
    {'3', {0x41, 0x49, 0x49, 0x49, 0x36}},
    {'4', {0x18, 0x14, 0x12, 0x7F, 0x00}},
    {'5', {0x07, 0x49, 0x49, 0x59, 0x71}},
    {'6', {0x3E, 0x49, 0x49, 0x49, 0x71}},
    {'7', {0x01, 0x01, 0x79, 0x05, 0x03}},
    {'9', {0x06, 0x49, 0x49, 0x49, 0x77}},
    {'.', {0x00, 0x40, 0x00, 0x00, 0x00}},
    {'!', {0x00, 0x00, 0x5F, 0x00, 0x00}},
};

// LED-L009 extension: lowercase a–z (standard 5×7 letterforms, documented NEW
// data in the same column format).
constexpr Glyph kLowercaseGlyphs[] = {
    {'a', {0x20, 0x54, 0x54, 0x54, 0x78}},
    {'b', {0x7F, 0x48, 0x48, 0x48, 0x30}},
    {'c', {0x30, 0x48, 0x48, 0x48, 0x00}},
    {'d', {0x30, 0x48, 0x48, 0x48, 0x7F}},
    {'e', {0x38, 0x44, 0x54, 0x54, 0x58}},
    {'f', {0x08, 0x7E, 0x09, 0x09, 0x00}},
    {'g', {0x18, 0x24, 0x24, 0x24, 0x7C}},
    {'h', {0x7F, 0x04, 0x04, 0x04, 0x78}},
    {'i', {0x00, 0x00, 0x7A, 0x00, 0x00}},
    {'j', {0x00, 0x40, 0x40, 0x40, 0x7A}},
    {'k', {0x7F, 0x10, 0x28, 0x6C, 0x44}},
    {'l', {0x00, 0x00, 0x7F, 0x00, 0x00}},
    {'m', {0x78, 0x08, 0x78, 0x08, 0x78}},
    {'n', {0x78, 0x08, 0x08, 0x08, 0x70}},
    {'o', {0x30, 0x48, 0x48, 0x48, 0x30}},
    {'p', {0x7C, 0x48, 0x48, 0x48, 0x30}},
    {'q', {0x30, 0x48, 0x48, 0x48, 0x7C}},
    {'r', {0x78, 0x08, 0x08, 0x00, 0x00}},
    {'s', {0x50, 0x68, 0x68, 0x68, 0x08}},
    {'t', {0x04, 0x7F, 0x44, 0x44, 0x00}},
    {'u', {0x78, 0x40, 0x40, 0x40, 0x7C}},  // V-L009-2 corrected (was a solid block)
    {'v', {0x1C, 0x20, 0x40, 0x20, 0x1C}},
    {'w', {0x78, 0x40, 0x20, 0x40, 0x78}},  // V-L009-2 corrected (was a solid block)
    {'x', {0x08, 0x50, 0x20, 0x50, 0x08}},
    {'y', {0x18, 0x18, 0x18, 0x18, 0x78}},
    {'z', {0x48, 0x48, 0x68, 0x38, 0x48}},
};

// LED-L009 extension: additional common punctuation (documented NEW data).
constexpr Glyph kPunctuationGlyphs[] = {
    {',', {0x00, 0x00, 0x00, 0x40, 0x20}},
    {'?', {0x02, 0x01, 0x51, 0x09, 0x06}},
    {':', {0x00, 0x00, 0x24, 0x00, 0x00}},
    {';', {0x00, 0x00, 0x24, 0x20, 0x40}},
    {'\'', {0x00, 0x00, 0x03, 0x00, 0x00}},
    {'"', {0x00, 0x03, 0x00, 0x03, 0x00}},
    {'(', {0x00, 0x38, 0x22, 0x41, 0x00}},
    {')', {0x41, 0x22, 0x38, 0x00, 0x00}},
    {'+', {0x10, 0x10, 0x7C, 0x10, 0x10}},
    {'=', {0x48, 0x48, 0x48, 0x48, 0x48}},
    {'%', {0x26, 0x10, 0x08, 0x04, 0x32}},
    {'/', {0x10, 0x08, 0x04, 0x02, 0x01}},
    {'*', {0x04, 0x08, 0x1E, 0x08, 0x04}},
    {'#', {0x14, 0x3E, 0x14, 0x3E, 0x14}},
    {'_', {0x40, 0x40, 0x40, 0x40, 0x40}},
    {'<', {0x00, 0x08, 0x14, 0x22, 0x00}},
    {'>', {0x00, 0x22, 0x14, 0x08, 0x00}},
};

constexpr uint8_t kHeritageCount =
    static_cast<uint8_t>(sizeof(kHeritageGlyphs) / sizeof(kHeritageGlyphs[0]));
constexpr uint8_t kExtensionCount =
    static_cast<uint8_t>(sizeof(kExtensionGlyphs) / sizeof(kExtensionGlyphs[0]));
constexpr uint8_t kLowercaseCount =
    static_cast<uint8_t>(sizeof(kLowercaseGlyphs) / sizeof(kLowercaseGlyphs[0]));
constexpr uint8_t kPunctuationCount =
    static_cast<uint8_t>(sizeof(kPunctuationGlyphs) / sizeof(kPunctuationGlyphs[0]));

// LED-L009 extension: non-ASCII characters by Unicode code point (Albanian
// Ë ë Ç ç; documented NEW data). Lookup is per decoded code point — the text
// pipeline decodes UTF-8 before consulting the font.
struct CodepointGlyph {
  uint32_t cp;
  uint8_t cols[5];
};

constexpr CodepointGlyph kCodepointGlyphs[] = {
    {0x00CB, {0x7E, 0x4B, 0x4A, 0x4B, 0x42}},  // Ë
    {0x00EB, {0x38, 0x45, 0x54, 0x55, 0x58}},  // ë
    {0x00C7, {0x3E, 0x21, 0x61, 0x21, 0x21}},  // Ç
    {0x00E7, {0x18, 0x24, 0x64, 0x24, 0x24}},  // ç
};
constexpr uint8_t kCodepointCount =
    static_cast<uint8_t>(sizeof(kCodepointGlyphs) / sizeof(kCodepointGlyphs[0]));

const Glyph* find_glyph(char c) {
  for (uint8_t i = 0; i < kHeritageCount; ++i) {
    if (kHeritageGlyphs[i].c == c) {
      return &kHeritageGlyphs[i];
    }
  }
  for (uint8_t i = 0; i < kExtensionCount; ++i) {
    if (kExtensionGlyphs[i].c == c) {
      return &kExtensionGlyphs[i];
    }
  }
  for (uint8_t i = 0; i < kLowercaseCount; ++i) {
    if (kLowercaseGlyphs[i].c == c) {
      return &kLowercaseGlyphs[i];
    }
  }
  for (uint8_t i = 0; i < kPunctuationCount; ++i) {
    if (kPunctuationGlyphs[i].c == c) {
      return &kPunctuationGlyphs[i];
    }
  }
  return nullptr;
}

const CodepointGlyph* find_glyph_cp(uint32_t cp) {
  for (uint8_t i = 0; i < kCodepointCount; ++i) {
    if (kCodepointGlyphs[i].cp == cp) {
      return &kCodepointGlyphs[i];
    }
  }
  return nullptr;
}

// Sentinel for invalid UTF-8 bytes: a code point outside every Unicode range,
// therefore guaranteed unsupported (renders as space, counted as skipped).
constexpr uint32_t kInvalidCodepoint = 0xFFFFFFFFu;

}  // namespace

size_t utf8_decode(const char* p, uint32_t* cp) {
  *cp = kInvalidCodepoint;
  if (p == nullptr || *p == '\0') {
    return 0;  // nothing to decode (callers treat 0 as end of string)
  }
  const uint8_t b0 = static_cast<uint8_t>(p[0]);
  if (b0 < 0x80) {  // ASCII fast path (includes NUL? no — NUL returned above)
    *cp = b0;
    return 1;
  }
  size_t len = 0;
  uint32_t value = 0;
  if ((b0 & 0xE0) == 0xC0) {
    len = 2;
    value = b0 & 0x1Fu;
  } else if ((b0 & 0xF0) == 0xE0) {
    len = 3;
    value = b0 & 0x0Fu;
  } else if ((b0 & 0xF8) == 0xF0) {
    len = 4;
    value = b0 & 0x07u;
  } else {
    return 1;  // stray continuation byte or 5+ byte sequence: unsupported
  }
  uint32_t acc = value;
  for (size_t i = 1; i < len; ++i) {
    const uint8_t b = static_cast<uint8_t>(p[i]);
    if ((b & 0xC0) != 0x80) {
      return 1;  // truncated/invalid: consume the lead byte only
    }
    acc = (acc << 6) | (b & 0x3Fu);
  }
  // Overlong/range checks (correct UTF-8 only; matches the supported-set needs).
  if ((len == 2 && acc < 0x80) || (len == 3 && acc < 0x800) ||
      (len == 4 && (acc < 0x10000 || acc > 0x10FFFF))) {
    return 1;
  }
  *cp = acc;
  return len;
}

bool glyph_supported(char c) {
  return c == ' ' || find_glyph(c) != nullptr;  // space is in the frozen set
}

void glyph_cols(char c, uint8_t cols[kGlyphWidth]) {
  cols[0] = cols[1] = cols[2] = cols[3] = cols[4] = 0;
  const Glyph* g = find_glyph(c);
  if (g != nullptr) {
    for (uint8_t i = 0; i < kGlyphWidth; ++i) {
      cols[i] = g->cols[i];
    }
  }
  // Unsupported characters render as space (all-zero columns) per frozen §6;
  // the caller counts them via count_skipped_characters().
}

bool glyph_supported_cp(uint32_t cp) {
  if (cp < 0x80) {
    return glyph_supported(static_cast<char>(cp));
  }
  return find_glyph_cp(cp) != nullptr;
}

void glyph_cols_cp(uint32_t cp, uint8_t cols[kGlyphWidth]) {
  cols[0] = cols[1] = cols[2] = cols[3] = cols[4] = 0;
  if (cp < 0x80) {
    glyph_cols(static_cast<char>(cp), cols);
    return;
  }
  const CodepointGlyph* g = find_glyph_cp(cp);
  if (g != nullptr) {
    for (uint8_t i = 0; i < kGlyphWidth; ++i) {
      cols[i] = g->cols[i];
    }
  }
}

}  // namespace text
}  // namespace ledwall
