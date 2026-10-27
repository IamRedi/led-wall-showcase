#include "render/frame_throttle.h"
#include "test_framework.h"

// Frame-throttle coverage (review finding R-N03): heritage dirty-flag + 50 ms
// floor semantics with independently derived boundary expectations. The `<`
// comparison against the last-show time is the heritage behavior; the unsigned
// subtraction is wrap-safe by construction.

void test_throttle_not_dirty_never_frames() {
  ledwall::FrameThrottle t;
  t.did_show(1000);  // clear dirty
  CHECK(!t.dirty());
  CHECK(!t.take_frame(1050));
  CHECK(!t.take_frame(5000));
  CHECK(!t.take_frame(0xFFFFFFFFu));  // even far in the future/wrap
}

void test_throttle_initial_dirty_before_floor() {
  ledwall::FrameThrottle t;  // dirty=true, last_show=0 (heritage initial state)
  CHECK(t.dirty());
  CHECK(!t.take_frame(0));   // 0 ms since last show: below the 50 ms floor
  CHECK(!t.take_frame(49));  // still below
}

void test_throttle_exactly_at_floor() {
  ledwall::FrameThrottle t;
  CHECK(t.take_frame(50));  // exactly 50 ms: frame due (heritage `<` compare)
}

void test_throttle_after_show_exact_and_over_floor() {
  ledwall::FrameThrottle t;
  CHECK(t.take_frame(50));
  t.did_show(50);  // dirty cleared, last_show=50
  CHECK(!t.dirty());
  CHECK(!t.take_frame(99));  // dirty=false wins regardless of elapsed time
  t.mark_dirty();
  CHECK(t.dirty());
  CHECK(!t.take_frame(99));  // dirty but only 49 ms since last show
  CHECK(t.take_frame(100));  // exactly 50 ms since last show
  t.did_show(100);
  CHECK(!t.dirty());
  t.mark_dirty();
  CHECK(t.take_frame(151));  // 51 ms since last show
  t.did_show(151);
  t.mark_dirty();
  CHECK(!t.take_frame(152));  // 1 ms since last show
}

void test_throttle_wrap_around() {
  ledwall::FrameThrottle t;
  t.did_show(0xFFFFFFFDu);  // last show 3 ticks before the rollover
  t.mark_dirty();
  CHECK(!t.take_frame(46));  // (46 - 0xFFFFFFFD) mod 2^32 = 49 ms: below floor
  CHECK(t.take_frame(47));   // 50 ms: frame due across the wrap
  t.did_show(47);
  CHECK(!t.dirty());
}

void test_throttle_did_show_clears_dirty() {
  ledwall::FrameThrottle t;
  CHECK(t.dirty());  // initial
  t.did_show(7);
  CHECK(!t.dirty());
  t.mark_dirty();
  CHECK(t.dirty());
  t.did_show(60);
  CHECK(!t.dirty());
  CHECK(!t.take_frame(200));  // no dirty → no frame despite elapsed time
}
