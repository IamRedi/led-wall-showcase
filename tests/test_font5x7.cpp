#include "text/font5x7.h"
#include "test_framework.h"

// Glyph-table conformance (frozen §6 font contract). Heritage expectations are
// byte-for-byte copies of `PLUTO EQ ACCESSORY/src/main.cpp glyphCols()` (the
// panel-verified text capability); extension glyphs are documented NEW data
// derived from standard 5×7 letterforms. Expectations are transcribed here
// independently of the implementation tables.

namespace {

struct ExpectedGlyph {
  char c;
  uint8_t cols[5];
};

// The heritage table, transcribed from the legacy source (read-only evidence).
constexpr ExpectedGlyph kHeritage[] = {
    {'A', {0x7E, 0x09, 0x09, 0x7E, 0x00}},
    {'E', {0x7F, 0x49, 0x49, 0x49, 0x41}},
    {'H', {0x7F, 0x08, 0x08, 0x08, 0x7F}},
    {'I', {0x00, 0x41, 0x7F, 0x41, 0x00}},
    {'L', {0x7F, 0x40, 0x40, 0x40, 0x40}},
    // D-L008-4 corrected bytes (render-verified on the physical panel during
    // the LED-L008 campaign; the heritage table itself carried malformed M/R).
    {'M', {0x7F, 0x02, 0x0C, 0x02, 0x7F}},
    {'N', {0x7F, 0x04, 0x08, 0x10, 0x7F}},
    {'O', {0x3E, 0x41, 0x41, 0x41, 0x3E}},
    {'P', {0x7F, 0x09, 0x09, 0x06, 0x00}},
    // D-L008-4: 'R' was byte-identical to 'P'; corrected on hardware.
    {'R', {0x7F, 0x09, 0x19, 0x29, 0x46}},
    {'T', {0x00, 0x01, 0x7F, 0x01, 0x00}},
    // V-L009-2 (LED-L009 repair cycle 2): this golden previously pinned the
    // heritage bytes 3E 41 41 41 3E — byte-identical to 'O'. The HERITAGE
    // SOURCE ITSELF carries U == O (legacy glyphCols case 'U' = case 'O',
    // verified in the read-only legacy repository) — the same class of
    // heritage data corruption as the D-L008-4 M/R defects, invisible until
    // the Owner read static letters at visual acceptance. Corrected to the
    // conventional 5×7 U: open at the top, connected bottom.
    {'U', {0x3F, 0x40, 0x40, 0x40, 0x3F}},
    {'0', {0x3E, 0x41, 0x41, 0x41, 0x3E}},
    {'8', {0x7F, 0x49, 0x49, 0x49, 0x7F}},
    {'-', {0x00, 0x00, 0x08, 0x08, 0x00}},
};

bool cols_equal(const uint8_t* a, const uint8_t* b) {
  for (int i = 0; i < 5; ++i) {
    if (a[i] != b[i]) {
      return false;
    }
  }
  return true;
}

}  // namespace

void test_font_heritage_glyphs_byte_identical() {
  for (const ExpectedGlyph& g : kHeritage) {
    uint8_t cols[ledwall::text::kGlyphWidth];
    ledwall::text::glyph_cols(g.c, cols);
    CHECK(cols_equal(cols, g.cols));
  }
}

void test_font_frozen_set_fully_supported() {
  // Frozen §6 guarantee: A–Z, 0–9, space, basic punctuation per the glyph
  // table. Space is supported (empty glyph).
  for (char c = 'A'; c <= 'Z'; ++c) {
    CHECK(ledwall::text::glyph_supported(c));
  }
  for (char c = '0'; c <= '9'; ++c) {
    CHECK(ledwall::text::glyph_supported(c));
  }
  CHECK(ledwall::text::glyph_supported(' '));
  CHECK(ledwall::text::glyph_supported('-'));
  CHECK(ledwall::text::glyph_supported('.'));
  CHECK(ledwall::text::glyph_supported('!'));
}

void test_font_extended_charset_supported() {
  // LED-L009 extended charset: lowercase a–z, Albanian Ë ë Ç ç, punctuation.
  for (char c = 'a'; c <= 'z'; ++c) {
    CHECK(ledwall::text::glyph_supported(c));
  }
  CHECK(ledwall::text::glyph_supported_cp(0x00CB));  // Ë
  CHECK(ledwall::text::glyph_supported_cp(0x00EB));  // ë
  CHECK(ledwall::text::glyph_supported_cp(0x00C7));  // Ç
  CHECK(ledwall::text::glyph_supported_cp(0x00E7));  // ç
  const char* punct = ",?:;'\"()+=%/*#_<>-.";
  for (const char* p = punct; *p; ++p) {
    CHECK(ledwall::text::glyph_supported(*p));
  }
}

void test_font_utf8_decode() {
  uint32_t cp = 0;
  CHECK_EQ_INT(ledwall::text::utf8_decode("A", &cp), 1);
  CHECK_EQ_INT(cp, 'A');
  CHECK_EQ_INT(ledwall::text::utf8_decode("\xC3\x8B", &cp), 2);  // Ë
  CHECK_EQ_INT(cp, 0x00CB);
  CHECK_EQ_INT(ledwall::text::utf8_decode("\xC3\xA7", &cp), 2);  // ç
  CHECK_EQ_INT(cp, 0x00E7);
  // Invalid sequences: 1 byte consumed, sentinel code point (unsupported).
  CHECK_EQ_INT(ledwall::text::utf8_decode("\xFF", &cp), 1);
  CHECK(!ledwall::text::glyph_supported_cp(cp));
  CHECK_EQ_INT(ledwall::text::utf8_decode("\xC3", &cp), 1);       // truncated
  CHECK(!ledwall::text::glyph_supported_cp(cp));
  CHECK_EQ_INT(ledwall::text::utf8_decode("\x80", &cp), 1);       // stray continuation
  CHECK(!ledwall::text::glyph_supported_cp(cp));
  CHECK_EQ_INT(ledwall::text::utf8_decode("", &cp), 0);            // end of string
}

void test_font_unsupported_characters() {
  // Symbols outside the (extended) supported set, control bytes, and raw
  // UTF-8 bytes through the byte API remain unsupported and deterministic.
  CHECK(!ledwall::text::glyph_supported('{'));
  CHECK(!ledwall::text::glyph_supported('&'));
  CHECK(!ledwall::text::glyph_supported('@'));
  CHECK(!ledwall::text::glyph_supported('^'));
  CHECK(!ledwall::text::glyph_supported('~'));
  CHECK(!ledwall::text::glyph_supported('\\'));
  CHECK(!ledwall::text::glyph_supported('\n'));
  CHECK(!ledwall::text::glyph_supported(static_cast<char>(0xC3)));  // UTF-8 lead byte
  CHECK(!ledwall::text::glyph_supported(static_cast<char>(0x80)));  // UTF-8 continuation
}

void test_font_unsupported_renders_as_space() {
  // §6: unsupported characters render as space (all-zero columns), never the
  // heritage box fallback.
  const char unsupported[] = {'{', '&', static_cast<char>(0xC3), static_cast<char>(0xFF)};
  for (char c : unsupported) {
    uint8_t cols[ledwall::text::kGlyphWidth];
    ledwall::text::glyph_cols(c, cols);
    for (int i = 0; i < 5; ++i) {
      CHECK_EQ_INT(cols[i], 0);
    }
  }
  uint8_t space[ledwall::text::kGlyphWidth];
  ledwall::text::glyph_cols(' ', space);
  for (int i = 0; i < 5; ++i) {
    CHECK_EQ_INT(space[i], 0);
  }
}

void test_font_d_extension_glyph_corrected() {
  // D-L008-4: 'D' in the extension table was malformed; corrected bytes.
  uint8_t cols[ledwall::text::kGlyphWidth];
  ledwall::text::glyph_cols('D', cols);
  const uint8_t expected[5] = {0x7F, 0x41, 0x41, 0x41, 0x3E};
  for (int i = 0; i < 5; ++i) {
    CHECK_EQ_INT(cols[i], expected[i]);
  }
}

void test_font_geometry() {
  CHECK_EQ_INT(ledwall::text::kGlyphWidth, 5);   // heritage Font5x7::kWidth
  CHECK_EQ_INT(ledwall::text::kGlyphHeight, 7);  // heritage Font5x7::kHeight
  CHECK_EQ_INT(ledwall::text::kAdvancePx, 6);    // heritage drawString advance
}

// ------------------------------------------------------------- V-L009-2 ----
void test_font_u_w_letterforms_owner_corrected() {
  uint8_t u[5];
  uint8_t w[5];
  uint8_t o[5];
  uint8_t m[5];
  uint8_t v[5];
  ledwall::text::glyph_cols('U', u);
  ledwall::text::glyph_cols('W', w);
  ledwall::text::glyph_cols('O', o);
  ledwall::text::glyph_cols('M', m);
  ledwall::text::glyph_cols('V', v);

  // U is no longer O (the heritage defect): different bytes entirely.
  CHECK(!cols_equal(u, o));
  // U is unambiguously U-shaped: both side columns full-height (open top,
  // solid sides), middle columns only the bottom row (connected bottom, no
  // closed top ring).
  CHECK_EQ_INT(u[0], 0x3F);  // rows 0..5: side open at the bottom corner
  CHECK_EQ_INT(u[4], 0x3F);
  CHECK_EQ_INT(u[1], 0x40);  // middle columns: bottom row only
  CHECK_EQ_INT(u[2], 0x40);
  CHECK_EQ_INT(u[3], 0x40);
  CHECK((u[2] & 0x01) == 0);  // no top pixel in the middle column (open top)

  // W is a double-V: full side strokes, a raised middle apex (rows 2..4) and
  // valley strokes — distinct from U (no middle pixels), M (upper-half inner
  // pixels only) and V (tapered bottom-heavy sides, no middle apex).
  CHECK_EQ_INT(w[0], 0x7F);
  CHECK_EQ_INT(w[4], 0x7F);
  CHECK((w[2] & 0x1C) == 0x1C);  // middle apex at rows 2,3,4
  CHECK((w[1] & 0x28) == 0x28);  // valley strokes at rows 3,5
  CHECK(!cols_equal(w, u));
  CHECK(!cols_equal(w, m));
  CHECK(!cols_equal(w, v));
  CHECK(!cols_equal(w, o));

  // Lowercase u/w: real letterforms (the previous entries were solid blocks).
  uint8_t lu[5];
  uint8_t lw[5];
  ledwall::text::glyph_cols('u', lu);
  ledwall::text::glyph_cols('w', lw);
  // u: stems and bottom, open at the top (middle columns lack top rows).
  CHECK_EQ_INT(lu[1], 0x40);  // middle stem columns: bottom row only
  CHECK((lu[0] & 0x04) == 0);  // left stem does not reach x-height top row 2
  CHECK((lu[4] & 0x7C) == 0x7C);  // right stem rises (classic u hook)
  // w: two V strokes — no solid middle rows (the previous blob is gone).
  CHECK_EQ_INT(lw[0], 0x78);  // left stem rows 3..6
  CHECK_EQ_INT(lw[4], 0x78);
  CHECK_EQ_INT(lw[2], 0x20);  // middle apex: single row-5 pixel, not a block
  CHECK_EQ_INT(lw[1], 0x40);  // valley bottoms at row 6
  CHECK(!cols_equal(lw, lu));
}
