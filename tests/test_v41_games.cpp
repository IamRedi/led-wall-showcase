#include "test_framework.h"

#include "api/registry.h"
#include "games/game_host.h"

using namespace ledwall;
using namespace ledwall::games;

void test_v41_games_bind_and_terminate() {
  CHECK_EQ_INT(kGameCount, 14);
  CHECK(game_max_players("tail_tennis") == 2);
  CHECK(game_max_players("light_quest") == 1);
  CHECK(game_min_lane("tail_tennis") == kTailGameMinLane);
  CHECK(game_min_lane("light_quest") == kTailGameMinLane);
  CHECK(game_by_index(11) == game_by_name("tail_tennis"));
  CHECK(game_by_index(12) == game_by_name("light_quest"));
  CHECK(game_by_index(13) == game_by_name("quick_draw"));
  CHECK_EQ_INT(game_min_players("quick_draw"), 2);
  CHECK_EQ_INT(game_max_players("reaction_stop"), 2);
  CHECK_EQ_INT(game_max_players("memory"), 2);

  Game* tennis = game_by_name("tail_tennis");
  CHECK(tennis != nullptr);
  tennis->set_lane(60);
  tennis->set_players(2);
  tennis->set_difficulty(GameDifficulty::Normal);
  tennis->reset(0);
  tennis->set_phase(GamePhase::Running);
  FrameBuffer countdown, expected_countdown;
  tennis->render(countdown, 0);
  render_centered_label(expected_countdown, "3", Rgb{230, 230, 220});
  for (uint8_t y = 0; y < 10; ++y)
    for (uint8_t x = 0; x < 30; ++x)
      CHECK(countdown.at(x, y) == expected_countdown.at(x, y));
  // P2 cannot hit a left-bound ball; P1 must hit near their own endpoint.
  tennis->on_input(2, InputEvent::Confirm, 0);
  for (int step = 0; step < 4000 && tennis->phase() != GamePhase::Over; ++step) {
    tennis->on_step(step * 100u);
    tennis->on_input(1, InputEvent::Confirm, step * 100u);
  }
  CHECK(tennis->phase() == GamePhase::Over);
  CHECK(tennis->score() >= 5);
  CHECK(tennis->player_score(1) == 5u || tennis->player_score(2) == 5u);
  FrameBuffer matrix;
  TailFramebuffer tail;
  tail.configure(60);
  tennis->render(matrix, 0);
  tennis->render_tail(tail, 0);
  CHECK(tail.at(0) != kRgbBlack);
  CHECK(tail.at(59) != kRgbBlack);

  Game* quest = game_by_name("light_quest");
  CHECK(quest != nullptr);
  quest->set_lane(60);
  quest->reset(0);
  quest->set_phase(GamePhase::Running);
  // Stage 1 is movement and exit only; the engine advances the stage.
  for (int i = 0; i < 59; ++i) quest->on_input(1, InputEvent::Left, i);
  CHECK(quest->phase() == GamePhase::Running);
  CHECK_EQ_INT(quest->score(), 100);
  quest->render(matrix, 0);
  quest->render_tail(tail, 0);
  CHECK(tail.at(0) != kRgbBlack);

  Game* draw = game_by_name("quick_draw");
  CHECK(draw != nullptr);
  draw->set_players(2);
  draw->reset(0);
  draw->set_phase(GamePhase::Running);
  draw->on_step(100);  // start ESP32-owned random delay
  draw->on_input(1, InputEvent::Confirm, 200);  // false start
  for (uint32_t t = 200; t <= 4800; t += 100) draw->on_step(t);
  CHECK_EQ_INT(draw->player_score(1), 0);
  CHECK_EQ_INT(draw->player_score(2), 1);  // P1 false start awards P2
  CHECK_EQ_INT(draw->winner(), 0);        // five-point match continues
  for (uint32_t round = 1; round < 5; ++round) {
    const uint32_t now = 5000u + round * 6000u;
    draw->on_input(1, InputEvent::Confirm, now);  // another early P1 press
    draw->on_step(now + 5000u);                  // P2 earns this point
    CHECK_EQ_INT(draw->player_score(2), round + 1u);
    CHECK(draw->phase() == (round < 4 ? GamePhase::Running : GamePhase::Over));
  }

  Game* stop = game_by_name("reaction_stop");
  stop->set_players(2);
  stop->set_lane(60);
  stop->reset(0);
  stop->set_phase(GamePhase::Running);
  for (int attempt = 0; attempt < 10; ++attempt) {
    const uint8_t active = (attempt & 1) ? 2 : 1;
    stop->on_input(active == 1 ? 2 : 1, InputEvent::Confirm, attempt * 100u);
    CHECK_EQ_INT(stop->player_score(1), 0);
    stop->on_input(active, InputEvent::Confirm, attempt * 100u + 1);
  }
  CHECK(stop->phase() == GamePhase::Over);
  CHECK_EQ_INT(stop->winner(), 3);  // five equal misses per player

  GameHost host;
  host.begin(nullptr);
  CHECK(host.start_by_name("pong", 0));
  CHECK(host.handle(1, InputEvent::Confirm, 0));
  CHECK(host.phase() == GamePhase::Running);
  host.update(100);
  const uint32_t before_disconnect = host.score();
  CHECK(host.pause_for_disconnect());
  CHECK(host.phase() == GamePhase::Paused);
  host.update(5000);
  CHECK_EQ_INT(host.score(), before_disconnect);
  CHECK(!host.pause_for_disconnect());
  CHECK(host.handle(1, InputEvent::StartPause, 5000));
  CHECK(host.phase() == GamePhase::Running);
}
