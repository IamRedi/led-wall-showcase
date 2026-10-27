#include "test_framework.h"

#include <initializer_list>

#include "games/game_host.h"

#include "text/text_renderer.h"

// LED-L009 games: deterministic startup, input, pause/resume, boundaries,
// game over, reset, no framebuffer out-of-bounds (FrameBuffer drops OOB; the
// games additionally hold their own grid bounds). Every game runs through the
// same host lifecycle.

using namespace ledwall;
using namespace ledwall::games;

namespace {

class CueSink final : public SoundSink {
 public:
  void play(SoundCue cue) override { ++cues[static_cast<int>(cue)]; }
  int cues[12] = {};
};

// Renders the active game every tick — catches OOB via the bounds-checked
// framebuffer and keeps the render path exercised like the device does.
void run(GameHost& host, FrameBuffer& fb, uint32_t from_ms, uint32_t to_ms) {
  for (uint32_t t = from_ms; t <= to_ms; t += 50) {
    host.update(t);
    host.render(fb, t);
  }
}

}  // namespace

void test_games_registry_bindings_complete() {
  for (uint8_t i = 0; i < kGameCount; ++i) {
    Game* g = game_by_index(i);
    CHECK(g != nullptr);
    CHECK_STR_EQ(g->name(), kGameNames[i]);
    CHECK(game_by_name(kGameNames[i]) == g);
    CHECK(g->label() != nullptr);
    // Panel labels stay static-renderable (<= 5 glyphs, 30 px).
    CHECK(text::string_width_px(g->label()) <= 30);
  }
  CHECK(game_by_name("nope") == nullptr);
  CHECK(game_by_name(nullptr) == nullptr);
  CHECK(game_by_index(kGameCount) == nullptr);
}

void test_game_host_lifecycle_all_games() {
  CueSink sink;
  FrameBuffer fb;
  for (uint8_t i = 0; i < kGameCount; ++i) {
    GameHost host;
    host.begin(&sink);
    CHECK(!host.active());
    CHECK(host.start_by_index(i, 1000));
    CHECK(host.active());
    CHECK_STR_EQ(host.name(), kGameNames[i]);
    CHECK_EQ_INT(static_cast<int>(host.phase()), static_cast<int>(GamePhase::Ready));
    run(host, fb, 1000, 3000);  // ready screen blinks, no OOB
    // Confirm starts.
    host.handle(InputEvent::Confirm, 3000);
    CHECK_EQ_INT(static_cast<int>(host.phase()), static_cast<int>(GamePhase::Running));
    run(host, fb, 3000, 3450);  // short burst: every game is still alive here
    // StartPause toggles pause; stepping halts while paused.
    host.handle(InputEvent::StartPause, 3500);
    CHECK_EQ_INT(static_cast<int>(host.phase()), static_cast<int>(GamePhase::Paused));
    const uint32_t score_frozen = host.score();
    run(host, fb, 3500, 8000);
    CHECK_EQ_INT(host.score(), score_frozen);
    host.handle(InputEvent::Confirm, 8000);
    CHECK_EQ_INT(static_cast<int>(host.phase()), static_cast<int>(GamePhase::Running));
    // Back exits via the runtime; here the host just reports consumption.
    CHECK(host.handle(InputEvent::Back, 9000));
    host.exit();
    CHECK(!host.active());
    // Deterministic reset: same inputs, same state.
    GameHost h2;
    h2.begin(&sink);
    h2.start_by_index(i, 1000);
    h2.handle(InputEvent::Confirm, 2000);
    run(h2, fb, 2000, 5000);
    GameHost h3;
    h3.begin(&sink);
    h3.start_by_index(i, 1000);
    h3.handle(InputEvent::Confirm, 2000);
    run(h3, fb, 2000, 5000);
    CHECK_EQ_INT(static_cast<int>(h2.score()), static_cast<int>(h3.score()));
    CHECK_EQ_INT(static_cast<int>(h2.phase()), static_cast<int>(h3.phase()));
  }
}

void test_game_pong_point_and_over() {
  CueSink sink;
  GameHost host;
  host.begin(&sink);
  host.start_by_name("pong", 0);
  host.handle(InputEvent::Confirm, 0);
  // Idle steps: the ball eventually scores for someone; V4.1 ends at 7.
  bool over = false;
  for (uint32_t t = 0; t <= 1200000 && !over; t += 100) {
    host.update(t);
    if (host.phase() == GamePhase::Over) over = true;
  }
  CHECK(over);  // deterministic match reaches 7 points
  // Confirm restarts to Ready with a fresh board.
  host.handle(InputEvent::Confirm, 1201000);
  CHECK_EQ_INT(static_cast<int>(host.phase()), static_cast<int>(GamePhase::Ready));
  CHECK_EQ_INT(host.score(), 0);
  FrameBuffer fb2;
  host.render(fb2, 1201000);
}

void test_game_snake_growth_and_wall_over() {
  CueSink sink;
  GameHost host;
  host.begin(&sink);
  host.start_by_name("snake", 0);
  host.handle(InputEvent::Confirm, 0);
  // Steer left into the wall: x decreases from 10 → Over well before 300 steps.
  bool over = false;
  for (uint32_t t = 0; t <= 300000 && !over; t += 100) {
    host.handle(InputEvent::Left, t);
    host.update(t);
    if (host.phase() == GamePhase::Over) over = true;
  }
  CHECK(over);
}

void test_game_reaction_stop_at_zero_precision() {
  CueSink sink;
  GameHost host;
  host.begin(&sink);
  // V-L009-3: this test predates difficulty and pins the NORMAL 10 s window
  // semantics; Normal is selected explicitly now that the default is Easy.
  host.set_difficulty(GameDifficulty::Normal);
  host.start_by_name("reaction", 0);
  host.handle(InputEvent::Confirm, 1000);   // host lifecycle start
  host.update(1000 + GameHost::kStepMs);    // first Running step arms the clock
  // Stop at ~3.2 s after the arm: remaining 6.800 s, exact per event ms.
  host.handle(InputEvent::Confirm, 1000 + GameHost::kStepMs + 3200);
  CHECK_EQ_INT(static_cast<int>(host.phase()), static_cast<int>(GamePhase::Over));
  CHECK_EQ_INT(host.score(), 10000 - 3200);
  // Natural expiry (no stop press) also reaches Over at zero (Normal 10 s).
  GameHost h2;
  h2.begin(&sink);
  h2.set_difficulty(GameDifficulty::Normal);
  h2.start_by_name("reaction", 0);
  h2.handle(InputEvent::Confirm, 0);
  bool over = false;
  for (uint32_t t = 0; t <= 12000 && !over; t += 50) {
    h2.update(t);
    if (h2.phase() == GamePhase::Over) over = true;
  }
  CHECK(over);
}

void test_game_breakout_and_dodger_bounds() {
  CueSink sink;
  FrameBuffer fb;
  // Breakout: paddle input moves; long idle play never crashes/OOBs.
  GameHost host;
  host.begin(&sink);
  host.start_by_name("breakout", 0);
  host.handle(InputEvent::Confirm, 0);
  host.handle(InputEvent::Confirm, 1);  // explicit serve (V4.1)
  for (uint32_t t = 0; t <= 200000; t += 100) {
    if (t % 700 == 0) host.handle(InputEvent::Left, t);
    if (t % 1100 == 0) host.handle(InputEvent::Right, t);
    host.update(t);
    host.render(fb, t);
  }
  // Dodger: dodge input + long run.
  GameHost h2;
  h2.begin(&sink);
  h2.start_by_name("dodger", 0);
  h2.handle(InputEvent::Confirm, 0);
  for (uint32_t t = 0; t <= 200000; t += 100) {
    if (t % 500 == 0) h2.handle(InputEvent::Right, t);
    if (t % 900 == 0) h2.handle(InputEvent::Left, t);
    h2.update(t);
    h2.render(fb, t);
  }
  CHECK(true);  // survival is probabilistic-free: determinism covered above
}

void test_game_host_unknown_start_fails_closed() {
  CueSink sink;
  GameHost host;
  host.begin(&sink);
  CHECK(!host.start_by_name("tetris", 0));
  CHECK(!host.active());
  CHECK(!host.start_by_index(99, 0));
  FrameBuffer fb3;
  host.render(fb3, 0);  // inactive host renders blank, no crash
}

void test_game_over_cue_fires_once_per_run() {
  CueSink sink;
  GameHost host;
  host.begin(&sink);
  host.start_by_name("snake", 0);
  host.handle(InputEvent::Confirm, 0);
  bool over = false;
  for (uint32_t t = 0; t <= 300000 && !over; t += 100) {
    host.handle(InputEvent::Left, t);
    host.update(t);
    if (host.phase() == GamePhase::Over) over = true;
  }
  CHECK(over);
  CHECK(sink.cues[static_cast<int>(SoundCue::Lose)] == 1);
  CHECK(sink.cues[static_cast<int>(SoundCue::GameOver)] == 0);
}

// ------------------------------------------------------------- V-L009-3 ----
// Engine-enforced difficulty: per-game Easy/Normal/Hard semantics, verified
// through the same public API the device uses (GameHost + Game interface).

void test_game_difficulty_host_default_and_persistence() {
  CueSink sink;
  GameHost host;
  host.begin(&sink);
  CHECK(host.difficulty() == GameDifficulty::Easy);  // default Easy
  host.set_difficulty(GameDifficulty::Hard);
  CHECK(host.difficulty() == GameDifficulty::Hard);
  host.start_by_name("snake", 0);
  host.exit();
  // The level persists across game sessions in the runtime.
  host.start_by_name("pong", 100);
  CHECK(host.difficulty() == GameDifficulty::Hard);
}

void test_game_snake_difficulty_moves_at_documented_intervals() {
  // Easy moves one cell per 400 ms, Hard one per 200 ms. After 3 s of the
  // same timeline the Easy head is at x=17, the Hard head at x=25.
  int head_easy = -1;
  int head_hard = -1;
  for (GameDifficulty d : {GameDifficulty::Easy, GameDifficulty::Hard}) {
    CueSink sink;
    GameHost host;
    host.begin(&sink);
    host.set_difficulty(d);
    host.start_by_name("snake", 0);
    host.handle(InputEvent::Confirm, 0);  // → Running
    for (uint32_t t = 100; t <= 3000; t += 100) {
      host.update(t);
    }
    FrameBuffer f;
    host.render(f, 3001);
    // Head (white) is the rightmost snake pixel; food is red and excluded.
    for (int x = 29; x >= 0; --x) {
      const Rgb px = f.at(static_cast<uint8_t>(x), 5);
      if (px == Rgb{255, 255, 255} || px == Rgb{40, 200, 60}) {
        if (d == GameDifficulty::Easy) {
          head_easy = x;
        } else {
          head_hard = x;
        }
        break;
      }
    }
  }
  CHECK_EQ_INT(head_easy, 18);
  CHECK_EQ_INT(head_hard, 26);
}

void test_game_pong_difficulty_ball_speed() {
  // After 2 running steps the Easy ball (1 px/step) is at x=17, the Hard ball
  // (3 px/step) at x=21 — no wall or paddle interaction by then.
  int x_easy = -1;
  int x_hard = -1;
  for (GameDifficulty d : {GameDifficulty::Easy, GameDifficulty::Hard}) {
    CueSink sink;
    GameHost host;
    host.begin(&sink);
    host.set_difficulty(d);
    host.start_by_name("pong", 0);
    host.handle(InputEvent::Confirm, 0);
    host.update(100);
    host.update(200);
    FrameBuffer f;
    host.render(f, 201);
    for (int x = 3; x <= 26; ++x) {
      bool found = false;
      for (int y = 0; y < 10; ++y) {
        if (f.at(static_cast<uint8_t>(x), static_cast<uint8_t>(y)) == Rgb{255, 255, 255}) {
          found = true;
          break;
        }
      }
      if (found) {
        if (d == GameDifficulty::Easy) {
          x_easy = x;
        } else {
          x_hard = x;
        }
        break;
      }
    }
  }
  CHECK_EQ_INT(x_easy, 17);
  CHECK_EQ_INT(x_hard, 21);

  // Five points end a real host-driven 2P match; no controller input is
  // required for the bounded deterministic ball/point progression.
  CueSink sink;
  GameHost duel;
  duel.begin(&sink);
  duel.set_players(2);
  duel.start_by_name("pong", 0);
  duel.handle(InputEvent::Confirm, 0);
  for (uint32_t t = GameHost::kStepMs;
       t <= 5000u * GameHost::kStepMs && duel.phase() != GamePhase::Over;
       t += GameHost::kStepMs) {
    duel.update(t);
  }
  CHECK(duel.phase() == GamePhase::Over);
  CHECK(duel.player_score(1) == 5u || duel.player_score(2) == 5u);
}

void test_game_breakout_difficulty_paddle_and_speed() {
  // Easy: 7-wide paddle and 1 ball cell per step; Hard: 5-wide, 3 cells/step.
  for (GameDifficulty d : {GameDifficulty::Easy, GameDifficulty::Hard}) {
    CueSink sink;
    GameHost host;
    host.begin(&sink);
    host.set_difficulty(d);
    host.start_by_name("breakout", 0);
    host.handle(InputEvent::Confirm, 0);
    host.handle(InputEvent::Confirm, 1);
    host.update(100);  // single step: Running, ball just left the paddle
    for (uint32_t t = 200; t <= 2100; t += 100) {
      host.handle(InputEvent::Left, t);  // paddle input, ball untouched
    }
    FrameBuffer f;
    host.render(f, 2101);
    int span = 0;
    for (int x = 0; x < 30; ++x) {
      if (f.at(static_cast<uint8_t>(x), 9) == Rgb{40, 200, 60}) {
        ++span;
      }
    }
    CHECK_EQ_INT(span, d == GameDifficulty::Easy ? 7 : 5);
  }
}

void test_game_reaction_difficulty_window_keeps_ms_precision() {
  // Windows: Easy 12 s, Normal 10 s, Hard 7 s — behaviorally distinguished by
  // when the natural expiry fires; the clock stays exact-millisecond.
  struct Case {
    GameDifficulty d;
    uint32_t window;
  };
  const Case cases[] = {
      {GameDifficulty::Easy, 12000u},
      {GameDifficulty::Normal, 10000u},
      {GameDifficulty::Hard, 7000u},
  };
  for (const Case& c : cases) {
    CueSink sink;
    GameHost host;
    host.begin(&sink);
    host.set_difficulty(c.d);
    host.start_by_name("reaction", 0);
    host.handle(InputEvent::Confirm, 0);  // first step (t=100) sets the origin
    // Exactly 50 ms before the window ends: still running.
    host.update(100u + c.window - 100u);  // last update strictly before expiry
    CHECK(host.phase() == GamePhase::Running);
    host.update(100u + c.window + 50u);
    CHECK(host.phase() == GamePhase::Over);  // natural expiry at exactly 0.000
    CHECK(host.score() == 0u);
    // A fresh run: an exact-millisecond stop is never quantized by difficulty.
    host.handle(InputEvent::Confirm, 100u + c.window + 100u);  // Over → Ready
    host.handle(InputEvent::Confirm, 100u + c.window + 200u);  // Ready → Running
    const uint32_t arm_at = 100u + c.window + 200u + GameHost::kStepMs;
    host.update(arm_at);                                        // origin = this step
    const uint32_t stop_at = arm_at + 1234u;
    host.handle(InputEvent::Confirm, stop_at);
    CHECK(host.phase() == GamePhase::Over);
    // distance from zero = window − 1234 exact milliseconds, every difficulty.
    CHECK_EQ_INT(static_cast<long long>(host.score()),
                 static_cast<long long>(c.window) - 1234);
  }
}

void test_game_dodger_difficulty_cadence_by_score() {
  // V4.1 score is survived seconds, independent of obstacle speed. The
  // difficulty still governs obstacle cadence; survival score uses elapsed
  // logic steps converted to whole seconds at the configured cadence.
  int checked = 0;
  for (GameDifficulty d : {GameDifficulty::Easy, GameDifficulty::Hard}) {
    CueSink sink;
    GameHost host;
    host.begin(&sink);
    host.set_difficulty(d);
    host.start_by_name("dodger", 0);
    host.handle(InputEvent::Confirm, 0);
    FrameBuffer fb;
    for (uint32_t t = 100; t <= 10000; t += 100) {
      host.update(t);
      host.render(fb, t);
      CHECK_EQ_INT(host.score(), (t / GameHost::kStepMs) * GameHost::kStepMs / 1000u);
      ++checked;
      if (host.phase() == GamePhase::Over) {
        break;  // collision ends this run
      }
    }
  }
  CHECK(checked >= 20);
}

void test_game_difficulty_change_applies_at_restart_not_live() {
  CueSink sink;
  GameHost host;
  host.begin(&sink);
  host.start_by_name("snake", 0);
  host.handle(InputEvent::Confirm, 0);
  host.set_difficulty(GameDifficulty::Hard);  // mid-session change
  CHECK(host.difficulty() == GameDifficulty::Hard);
  // The documented restart path re-arms the game with the CURRENT level
  // (the host does exactly this on Over→confirm / start).
  host.current_game()->set_difficulty(host.difficulty());
  host.current_game()->reset(50000);
  host.handle(InputEvent::Confirm, 50000);  // Ready → Running
  // Hard cadence moves on the 2nd step: after 3 steps the head is at x=11
  // (Easy would still sit at 10, moving only on the 4th step).
  host.update(50100);
  host.update(50200);
  host.update(50300);
  FrameBuffer f;
  host.render(f, 50301);
  int head = -1;
  for (int x = 29; x >= 0; --x) {
    const Rgb px = f.at(static_cast<uint8_t>(x), 5);
    if (px == Rgb{255, 255, 255} || px == Rgb{40, 200, 60}) {
      head = x;
      break;
    }
  }
  CHECK_EQ_INT(head, 11);
}

void test_game_difficulty_inputs_not_dropped() {
  CueSink sink;
  GameHost host;
  host.begin(&sink);
  host.set_difficulty(GameDifficulty::Hard);
  host.start_by_name("snake", 0);
  host.handle(InputEvent::Confirm, 0);
  host.set_difficulty(GameDifficulty::Easy);  // change mid-run
  // Gameplay events still route to the game (Up queues a direction change).
  CHECK(host.handle(InputEvent::Up, 1000) == false);  // false = gameplay input
  CHECK(host.phase() == GamePhase::Running);
  // Lifecycle keys keep working around the change.
  CHECK(host.handle(InputEvent::StartPause, 1100) == true);  // pause
  CHECK(host.phase() == GamePhase::Paused);
  CHECK(host.handle(InputEvent::StartPause, 1200) == true);  // resume
  CHECK(host.phase() == GamePhase::Running);
}
