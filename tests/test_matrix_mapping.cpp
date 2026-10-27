#include "render/matrix_mapping.h"
#include "test_framework.h"

// Matrix mapping must stay byte-faithful to the heritage serpentine layout
// (frozen spec §2/§12; INHERITED-PRESERVE per SOURCE_MAP). Panel verification
// on the physical wall is a §22 hardware gate outside LED-L002 authorization.

void test_matrix_mapping_expected_indices() {
  // Heritage flags: flip_odd_rows=true, flip_y=false. Row 0 runs left→right,
  // odd rows reversed; row 1 ends directly under pixel 29.
  const ledwall::OrientationFlags heritage = ledwall::kHeritageOrientation;
  CHECK_EQ_INT(ledwall::screen_to_index(0, 0, heritage), 0);
  CHECK_EQ_INT(ledwall::screen_to_index(29, 0, heritage), 29);
  CHECK_EQ_INT(ledwall::screen_to_index(0, 1, heritage), 59);
  CHECK_EQ_INT(ledwall::screen_to_index(29, 1, heritage), 30);
  CHECK_EQ_INT(ledwall::screen_to_index(0, 2, heritage), 60);
  CHECK_EQ_INT(ledwall::screen_to_index(29, 2, heritage), 89);
  CHECK_EQ_INT(ledwall::screen_to_index(0, 3, heritage), 119);
  CHECK_EQ_INT(ledwall::screen_to_index(5, 3, heritage), 114);   // 90 + (29-5)
  CHECK_EQ_INT(ledwall::screen_to_index(0, 9, heritage), 299);   // 270 + (29-0)
  CHECK_EQ_INT(ledwall::screen_to_index(29, 9, heritage), 270);  // 270 + (29-29)
}

void test_matrix_mapping_bijection() {
  const ledwall::OrientationFlags variants[] = {
      {true, false}, {false, false}, {true, true}, {false, true},
  };
  for (const ledwall::OrientationFlags& flags : variants) {
    bool seen[ledwall::kLedCount] = {};
    for (uint8_t y = 0; y < ledwall::kMatrixHeight; ++y) {
      for (uint8_t x = 0; x < ledwall::kMatrixWidth; ++x) {
        const uint16_t idx = ledwall::screen_to_index(x, y, flags);
        CHECK(idx < ledwall::kLedCount);
        CHECK(!seen[idx]);  // every pixel maps to a unique LED
        seen[idx] = true;
      }
    }
    for (uint16_t i = 0; i < ledwall::kLedCount; ++i) {
      CHECK(seen[i]);  // every LED is reachable
    }
  }
}

void test_matrix_mapping_no_flip() {
  const ledwall::OrientationFlags straight{false, false};
  CHECK_EQ_INT(ledwall::screen_to_index(0, 1, straight), 30);
  CHECK_EQ_INT(ledwall::screen_to_index(29, 1, straight), 59);
  CHECK_EQ_INT(ledwall::screen_to_index(5, 3, straight), 95);  // 3*30 + 5
}

void test_matrix_mapping_flip_y() {
  const ledwall::OrientationFlags flip_y_only{false, true};
  CHECK_EQ_INT(ledwall::screen_to_index(0, 0, flip_y_only), 270);  // y → 9
  CHECK_EQ_INT(ledwall::screen_to_index(0, 9, flip_y_only), 0);    // y → 0

  const ledwall::OrientationFlags flip_y_odd{true, true};
  CHECK_EQ_INT(ledwall::screen_to_index(0, 0, flip_y_odd), 299);   // y → 9 (odd, reversed)
  CHECK_EQ_INT(ledwall::screen_to_index(29, 9, flip_y_odd), 29);   // y → 0 (row 0, x preserved)
}

void test_matrix_mapping_out_of_bounds() {
  const ledwall::OrientationFlags heritage = ledwall::kHeritageOrientation;
  CHECK_EQ_INT(ledwall::screen_to_index(30, 0, heritage), ledwall::kInvalidLedIndex);
  CHECK_EQ_INT(ledwall::screen_to_index(0, 10, heritage), ledwall::kInvalidLedIndex);
  CHECK_EQ_INT(ledwall::screen_to_index(255, 255, heritage), ledwall::kInvalidLedIndex);
}
