#pragma once

#include <cstddef>
#include <cstdint>

namespace ledwall {
namespace text {

// 5×7 bitmap font, column-major (left→right columns, bit0 = top row) — the
// heritage format of `PLUTO EQ ACCESSORY/src/main.cpp glyphCols()` (frozen
// spec §20 SOURCE_MAP: text rendering → COPY/ADAPT; §6: the exact font
// implementation is INTERNAL, the contract guarantees the supported set and
// unsupported-character reporting).
//
// Glyph data policy:
// - Characters defined by the heritage table are COPIED byte-for-byte, with
//   the three LED-L008 campaign corrections (D-L008-4: 'M', 'R' in the
//   heritage table and 'D' in the extension table were malformed and were
//   render-verified on the physical panel before the LED-L009 baseline).
// - The frozen §6 contract guarantees A–Z, 0–9, space, and basic punctuation.
//   LED-L009 extends the supported set additively: full lowercase a–z, the
//   Albanian letters Ë ë Ç ç (UTF-8 code points U+00CB/U+00EB/U+00C7/U+00E7),
//   and additional common punctuation. Heritage bytes are never re-derived.
// - The heritage box-fallback for unknown characters is SUPERSEDED by the
//   frozen §6 contract: unsupported characters render as space and are
//   counted in skippedCharacters (see text_renderer).
constexpr uint8_t kGlyphWidth = 5;   // heritage Font5x7::kWidth
constexpr uint8_t kGlyphHeight = 7;  // heritage Font5x7::kHeight
constexpr uint8_t kAdvancePx = 6;    // heritage drawString advance (5 + 1 spacing)

// Decodes one UTF-8 sequence starting at `p` (NUL-terminated string). Stores
// the code point in `*cp` and returns the number of bytes consumed (always
// >= 1). Invalid/overlong/truncated sequences consume exactly 1 byte and
// yield a sentinel code point that is guaranteed unsupported (renders as
// space, counted in skippedCharacters). ASCII passes through unchanged.
size_t utf8_decode(const char* p, uint32_t* cp);

// True iff `c` (a single byte) is in the supported set. Bytes >= 0x80 are
// never supported through this byte API — multi-byte characters must go
// through the code-point API below (the text pipeline always does).
bool glyph_supported(char c);

// Filled with the 5 column bytes of `c`; all-zero for space and for any
// unsupported byte (§6: rendered as space).
void glyph_cols(char c, uint8_t cols[kGlyphWidth]);

// Code-point variants (LED-L009): the text pipeline decodes UTF-8 and looks
// glyphs up per code point, so Albanian Ë/ë/Ç/ç render natively.
bool glyph_supported_cp(uint32_t cp);
void glyph_cols_cp(uint32_t cp, uint8_t cols[kGlyphWidth]);

}  // namespace text
}  // namespace ledwall
