#include "core/brightness.h"
#include "test_framework.h"

// Frozen spec §5/§13, ADR-011: semantic 0–100, locally enforced ceiling,
// requests clamped. Heritage evidence defaults (§2): 40.

void test_brightness_defaults_and_clamp() {
  ledwall::Brightness b;
  b.configure(40, 40);
  CHECK_EQ_INT(b.level(), 40);
  CHECK_EQ_INT(b.ceiling(), 40);
  CHECK_EQ_INT(b.requested(), 40);
  CHECK_EQ_INT(b.hardware_scale(), 102);  // round(40 * 255 / 100)
  b.clear_dirty();

  // Request above the ceiling: the request is stored, the level is clamped
  // locally — already at the ceiling, so no visual change and no dirty.
  CHECK(!b.set_level(100));
  CHECK_EQ_INT(b.requested(), 100);
  CHECK_EQ_INT(b.level(), 40);  // ceiling enforced
  CHECK_EQ_INT(b.hardware_scale(), 102);
  CHECK(!b.dirty());

  // Request above the semantic range: clamped to 100 first, still no change.
  CHECK(!b.set_level(250));
  CHECK_EQ_INT(b.requested(), 100);
  CHECK_EQ_INT(b.level(), 40);
  CHECK(!b.dirty());

  // No-op request: no change, no dirty.
  b.clear_dirty();
  CHECK(!b.set_level(40));
  CHECK_EQ_INT(b.level(), 40);
  CHECK(!b.dirty());

  CHECK(b.set_level(0));
  CHECK_EQ_INT(b.level(), 0);
  CHECK_EQ_INT(b.hardware_scale(), 0);
}

void test_brightness_ceiling_changes() {
  ledwall::Brightness b;
  b.configure(100, 40);
  CHECK_EQ_INT(b.level(), 40);

  // Raising the ceiling lets the existing request through up to it.
  CHECK(b.set_ceiling(80));
  CHECK_EQ_INT(b.ceiling(), 80);
  CHECK_EQ_INT(b.level(), 80);
  CHECK_EQ_INT(b.hardware_scale(), 204);  // round(80 * 255 / 100)

  // Full-scale ceiling with max request reaches FastLED 255.
  CHECK(b.set_ceiling(100));
  CHECK_EQ_INT(b.level(), 100);
  CHECK_EQ_INT(b.hardware_scale(), 255);

  // Lowering the ceiling re-clamps the active level immediately.
  CHECK(b.set_ceiling(10));
  CHECK_EQ_INT(b.level(), 10);
  CHECK_EQ_INT(b.requested(), 100);

  // Out-of-range ceiling config falls back to the semantic maximum; the
  // active level follows the stored request up to the new ceiling.
  CHECK(b.set_ceiling(250));
  CHECK_EQ_INT(b.ceiling(), 100);
  CHECK_EQ_INT(b.level(), 100);
  CHECK_EQ_INT(b.hardware_scale(), 255);

  // Unchanged ceiling: no dirty.
  b.clear_dirty();
  CHECK(!b.set_ceiling(100));
  CHECK(!b.dirty());
}
