#include "test_framework.h"

#include "timer/countdown.h"

// LED-L009 countdown state machine: IDLE/RUNNING/PAUSED/FINISHED, exact ms
// preservation across pause/resume, zero, wrap-safety, disconnect
// independence by construction (pure device state — no client involved).

using namespace ledwall;

void test_countdown_arm_and_idle_view() {
  CountdownTimer t;
  CHECK_EQ_INT(static_cast<int>(t.state()), static_cast<int>(CountdownTimer::State::Idle));
  t.arm(10000);
  CHECK_EQ_INT(static_cast<int>(t.state()), static_cast<int>(CountdownTimer::State::Idle));
  CHECK_EQ_INT(t.duration_ms(), 10000);
  CHECK_EQ_INT(t.remaining_ms(123456), 10000);  // idle shows the stored duration
  // A never-armed engine cannot start (no duration).
  CountdownTimer t0;
  CHECK(!t0.start(1000));
  CountdownTimer t2;
  t2.arm(5000);
  CHECK(t2.start(1000));
}

void test_countdown_run_pause_resume_exact_ms() {
  CountdownTimer t;
  t.arm(10000);
  CHECK(t.start(100000));
  CHECK_EQ_INT(static_cast<int>(t.state()), static_cast<int>(CountdownTimer::State::Running));
  CHECK_EQ_INT(t.remaining_ms(103000), 7000);
  CHECK_EQ_INT(t.remaining_ms(103354), 6646);  // exact ms, not rounded seconds
  // Pause at 7.354 remaining (brief example): display stays 7.354.
  CHECK(t.pause(103354));
  CHECK_EQ_INT(static_cast<int>(t.state()), static_cast<int>(CountdownTimer::State::Paused));
  CHECK_EQ_INT(t.remaining_ms(103354), 6646);
  CHECK_EQ_INT(t.remaining_ms(200000), 6646);  // frozen while paused
  // Resume continues from the frozen value, never from the duration.
  CHECK(t.resume(250000));
  CHECK_EQ_INT(static_cast<int>(t.state()), static_cast<int>(CountdownTimer::State::Running));
  CHECK_EQ_INT(t.remaining_ms(250000), 6646);
  CHECK_EQ_INT(t.remaining_ms(251000), 5646);
}

void test_countdown_zero_finishes_stable() {
  CountdownTimer t;
  t.arm(3000);
  t.start(1000);
  t.tick(3999);
  CHECK_EQ_INT(static_cast<int>(t.state()), static_cast<int>(CountdownTimer::State::Running));
  t.tick(4000);  // exactly at the deadline
  CHECK_EQ_INT(static_cast<int>(t.state()), static_cast<int>(CountdownTimer::State::Finished));
  CHECK_EQ_INT(t.remaining_ms(900000), 0);  // stable finished state, no wrap
  t.tick(99999999);
  CHECK_EQ_INT(static_cast<int>(t.state()), static_cast<int>(CountdownTimer::State::Finished));
  // Restart from the stored duration.
  CHECK(t.start(99999999));
  CHECK_EQ_INT(static_cast<int>(t.state()), static_cast<int>(CountdownTimer::State::Running));
  CHECK_EQ_INT(t.remaining_ms(99999999), 3000);
}

void test_countdown_reset_and_stop_distinction() {
  CountdownTimer t;
  t.arm(8000);
  t.start(0);
  t.pause(100);
  t.reset(500);  // re-arm: Idle with the stored duration
  CHECK_EQ_INT(static_cast<int>(t.state()), static_cast<int>(CountdownTimer::State::Idle));
  CHECK_EQ_INT(t.remaining_ms(900), 8000);
  CHECK(!t.pause(1000));  // pause from Idle is a no-op
  CHECK(!t.resume(1000));
  CHECK(t.start(1000));
  CHECK(t.pause(2000));
  t.reset(3000);
  CHECK_EQ_INT(t.remaining_ms(3001), 8000);  // full duration again
}

void test_countdown_rollover_wrap_safety() {
  CountdownTimer t;
  t.arm(5000);
  const uint32_t start = 0xFFFFFF00u;  // deadline wraps past 2^32
  CHECK(t.start(start));
  CHECK_EQ_INT(t.remaining_ms(0xFFFFFF00u + 100u), 4900);
  t.tick(0xFFFFFF00u + 4999u);
  CHECK_EQ_INT(static_cast<int>(t.state()), static_cast<int>(CountdownTimer::State::Running));
  t.tick(0xFFFFFF00u + 5000u);  // wrapped deadline (now reads ~744)
  CHECK_EQ_INT(static_cast<int>(t.state()), static_cast<int>(CountdownTimer::State::Finished));
  // Pause/resume across the wrap keeps exact ms.
  CountdownTimer u;
  u.arm(10000);
  u.start(0xFFFFFFF0u);
  CHECK(u.pause(20u));  // 16 ms ran before wrap + 20 after = 36 elapsed
  CHECK_EQ_INT(u.remaining_ms(1000), 9964);
  CHECK(u.resume(50000));
  CHECK_EQ_INT(u.remaining_ms(51000), 8964);
}

void test_countdown_arm_validation_and_bounds() {
  CountdownTimer t;
  t.arm(0);   // invalid: mutates nothing
  CHECK_EQ_INT(t.duration_ms(), 0);
  CHECK(!t.start(0));
  t.arm(CountdownTimer::kMaxDurationMs + 1);
  CHECK_EQ_INT(t.duration_ms(), 0);
  t.arm(5999001);
  t.arm(CountdownTimer::kMaxDurationMs);
  CHECK_EQ_INT(t.duration_ms(), CountdownTimer::kMaxDurationMs);
}

void test_countdown_display_formats() {
  char buf[9];
  countdown_format(0, buf);
  CHECK_STR_EQ(buf, "0.000");
  countdown_format(7354, buf);
  CHECK_STR_EQ(buf, "7.354");
  countdown_format(9999, buf);
  CHECK_STR_EQ(buf, "9.999");
  countdown_format(10000, buf);
  CHECK_STR_EQ(buf, "10");
  countdown_format(42000, buf);
  CHECK_STR_EQ(buf, "42");
  countdown_format(59999, buf);
  CHECK_STR_EQ(buf, "59");
  countdown_format(60000, buf);
  CHECK_STR_EQ(buf, "01:00");
  countdown_format(61000, buf);
  CHECK_STR_EQ(buf, "01:01");
  countdown_format(3600000 + 25000, buf);
  CHECK_STR_EQ(buf, "60:25");
  // All formats are <= 5 glyphs (30 px, static-renderable); the duration cap
  // is the largest MM:SS-displayable value.
  countdown_format(5999000, buf);
  CHECK_STR_EQ(buf, "99:59");
}

void test_countdown_state_names() {
  CHECK_STR_EQ(CountdownTimer::state_name(CountdownTimer::State::Idle), "idle");
  CHECK_STR_EQ(CountdownTimer::state_name(CountdownTimer::State::Running), "running");
  CHECK_STR_EQ(CountdownTimer::state_name(CountdownTimer::State::Paused), "paused");
  CHECK_STR_EQ(CountdownTimer::state_name(CountdownTimer::State::Finished), "finished");
}


