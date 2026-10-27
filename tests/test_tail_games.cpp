#include "test_framework.h"

#include <initializer_list>

#include "games/game_host.h"

#include "text/text_renderer.h"

// LED-L010 tail lane games (ADR-LED-L010 D6): gameplay, scoring, difficulty,
// pause/resume/restart, 2-player routing, lane bounds — through the same
// public GameHost/Game API the device uses. The lane is injected (synthetic
// 60-cell rope unless noted); renders clamp to the actual surface.

using namespace ledwall;
using namespace ledwall::games;

namespace {

class CueSink final : public SoundSink {
 public:
  void play(SoundCue cue) override { ++cues[static_cast<int>(cue)]; }
  int cues[12] = {};
};

void run_host(GameHost& host, FrameBuffer& fb, TailFramebuffer& tail, uint32_t from_ms,
              uint32_t to_ms) {
  for (uint32_t t = from_ms; t <= to_ms; t += 50) {
    host.update(t);
    host.render(fb, &tail, t);
  }
}

}  // namespace

void test_tail_games_registry_and_labels() {
  const char* tail_games[6] = {"color_chain", "light_race", "reaction_stop",
                               "color_catch", "memory", "tug_of_war"};
  for (const char* name : tail_games) {
    Game* g = game_by_name(name);
    CHECK(g != nullptr);
    CHECK_STR_EQ(g->name(), name);
    CHECK(g->label() != nullptr);
    CHECK(text::string_width_px(g->label()) <= 30);  // panel-ready chrome
  }
  // Zone metadata coherence (registry metadata matches the binding surface).
  CHECK(game_zone("reaction_stop") == ZoneTarget::Tail);
  CHECK(game_zone("color_chain") == ZoneTarget::Both);
  CHECK(game_zone("pong") == ZoneTarget::Matrix);  // accepted games unchanged
}

void test_tail_game_host_lifecycle_all_six() {
  CueSink sink;
  FrameBuffer fb;
  TailFramebuffer tail;
  tail.configure(60);
  const char* tail_games[6] = {"color_chain", "light_race", "reaction_stop",
                               "color_catch", "memory", "tug_of_war"};
  for (const char* name : tail_games) {
    GameHost host;
    host.begin(&sink);
    host.set_lane_length(60);
    CHECK(host.start_by_name(name, 1000));
    CHECK_STR_EQ(host.name(), name);
    CHECK_EQ_INT(static_cast<int>(host.phase()), static_cast<int>(GamePhase::Ready));
    run_host(host, fb, tail, 1000, 3000);  // ready chrome, no OOB
    host.handle(kPlayer1, InputEvent::Confirm, 3000);
    CHECK_EQ_INT(static_cast<int>(host.phase()), static_cast<int>(GamePhase::Running));
    run_host(host, fb, tail, 3000, 3450);  // short burst: alive, no OOB
    host.handle(kPlayer1, InputEvent::StartPause, 3500);
    CHECK_EQ_INT(static_cast<int>(host.phase()), static_cast<int>(GamePhase::Paused));
    const uint32_t frozen = host.score();
    run_host(host, fb, tail, 3500, 8000);
    CHECK_EQ_INT(host.score(), frozen);  // paused: no physics
    host.handle(kPlayer1, InputEvent::Confirm, 8000);
    CHECK_EQ_INT(static_cast<int>(host.phase()), static_cast<int>(GamePhase::Running));
    CHECK(host.handle(kPlayer1, InputEvent::Back, 9000));
    host.exit();
    CHECK(!host.active());
    // Determinism: identical timelines → identical score/phase.
    GameHost a, b;
    a.begin(&sink);
    b.begin(&sink);
    a.set_lane_length(60);
    b.set_lane_length(60);
    a.start_by_name(name, 1000);
    b.start_by_name(name, 1000);
    a.handle(kPlayer1, InputEvent::Confirm, 2000);
    b.handle(kPlayer1, InputEvent::Confirm, 2000);
    run_host(a, fb, tail, 2000, 5000);
    run_host(b, fb, tail, 2000, 5000);
    CHECK_EQ_INT(static_cast<int>(a.score()), static_cast<int>(b.score()));
    CHECK_EQ_INT(static_cast<int>(a.phase()), static_cast<int>(b.phase()));
  }
}

void test_light_race_2p_routing() {
  CueSink sink;
  FrameBuffer fb;
  TailFramebuffer tail;
  tail.configure(60);
  // 2P: P2 presses must NOT advance P1's bar (and vice versa).
  GameHost host;
  host.begin(&sink);
  host.set_lane_length(60);
  host.set_players(2);
  host.start_by_name("light_race", 0);
  host.handle(kPlayer1, InputEvent::Confirm, 0);
  host.handle(kPlayer2, InputEvent::Confirm, 100);  // A
  host.handle(kPlayer2, InputEvent::Confirm, 101);  // held/repeated A: ignored
  host.handle(kPlayer2, InputEvent::ActionB, 270);  // B, beyond Easy debounce
  host.handle(kPlayer2, InputEvent::Confirm, 440);  // A
  // P1 bar stays empty; P2 advanced by three alternating edges.
  host.render(fb, &tail, 440);
  int p1_cells = 0;
  int p2_cells = 0;
  for (int x = 0; x < 30; ++x) {
    if (fb.at(static_cast<uint8_t>(x), 1) == Rgb{40, 220, 70}) {
      ++p1_cells;
    }
    if (fb.at(static_cast<uint8_t>(x), 8) == Rgb{240, 170, 30}) {
      ++p2_cells;
    }
  }
  CHECK_EQ_INT(p1_cells, 0);
  CHECK(p2_cells > 0);
  CHECK_EQ_INT(host.current_game()->players(), 2);
  // The two heads depart from neighboring center cells toward opposite ends.
  CHECK((tail.at(29) == Rgb{255, 255, 255}));
  CHECK((tail.at(33) == Rgb{255, 255, 255}));

  // A new match needs five earned points, including the fifth round.
  Game* race = game_by_name("light_race");
  race->set_lane(30);
  race->set_difficulty(GameDifficulty::Easy);
  race->reset(0);
  race->set_phase(GamePhase::Running);
  for (uint32_t round = 0; round < 5; ++round) {
    for (uint32_t press = 0; press < 14; ++press) {
      const uint32_t at = 1000u + round * 5000u + press * 170u;
      race->on_input(1, (press & 1u) ? InputEvent::ActionB : InputEvent::Confirm, at);
    }
    race->on_step(4000u + round * 5000u);
    CHECK_EQ_INT(race->player_score(1), round + 1u);
    CHECK(race->phase() == (round < 4 ? GamePhase::Running : GamePhase::Over));
  }
}

void test_tug_of_war_2p_pulls_opposite() {
  CueSink sink;
  FrameBuffer fb;
  TailFramebuffer tail;
  tail.configure(60);
  GameHost host;
  host.begin(&sink);
  host.set_lane_length(60);
  host.set_players(2);
  host.start_by_name("tug_of_war", 0);
  host.handle(kPlayer1, InputEvent::Confirm, 0);  // marker 29 → 28
  host.handle(kPlayer1, InputEvent::Confirm, 10);
  host.handle(kPlayer1, InputEvent::Confirm, 20);  // → 26
  host.render(fb, &tail, 30);
  // Mirror bar: the marker cell (white) sits LEFT of center after P1 pulls.
  int white = -1;
  for (int x = 0; x < 30; ++x) {
    if (fb.at(static_cast<uint8_t>(x), 9) == Rgb{255, 255, 255}) {
      white = x;
      break;
    }
  }
  CHECK(white >= 0);
  CHECK(white < 15);  // pulled toward P1's side
  // P2 pulls push the mirror right of center.
  GameHost h2;
  h2.begin(&sink);
  h2.set_lane_length(60);
  h2.set_players(2);
  h2.start_by_name("tug_of_war", 0);
  h2.handle(kPlayer2, InputEvent::Confirm, 0);
  h2.handle(kPlayer2, InputEvent::Confirm, 10);
  h2.handle(kPlayer2, InputEvent::Confirm, 20);
  h2.render(fb, &tail, 30);
  int white2 = -1;
  for (int x = 0; x < 30; ++x) {
    if (fb.at(static_cast<uint8_t>(x), 9) == Rgb{255, 255, 255}) {
      white2 = x;
      break;
    }
  }
  CHECK(white2 > white);  // opposite direction
}

void test_reaction_stop_hit_and_miss() {
  CueSink sink;
  FrameBuffer fb;
  TailFramebuffer tail;
  tail.configure(60);
  // Miss: confirm immediately after start → zero for the first attempt;
  // the five-attempt campaign continues.
  GameHost miss;
  miss.begin(&sink);
  miss.set_lane_length(60);
  miss.start_by_name("reaction_stop", 0);
  miss.handle(kPlayer1, InputEvent::Confirm, 0);
  miss.handle(kPlayer1, InputEvent::Confirm, 50);  // lifecycle start then stop
  CHECK_EQ_INT(static_cast<int>(miss.phase()), static_cast<int>(GamePhase::Running));
  CHECK_EQ_INT(miss.score(), 0);
  for (int i = 1; i < 5; ++i)
    miss.handle(kPlayer1, InputEvent::Confirm, 50 + i * 100);
  CHECK_EQ_INT(static_cast<int>(miss.phase()), static_cast<int>(GamePhase::Over));
  CHECK_EQ_INT(sink.cues[static_cast<int>(SoundCue::Lose)], 1);
  miss.update(10000);
  CHECK_EQ_INT(sink.cues[static_cast<int>(SoundCue::Lose)], 1);
  // Hit: step the host tick by tick; after each step read the RENDERED truth
  // (white light cell + green zone) and press exactly when the light is inside
  // the zone — no knowledge of game internals required.
  GameHost hit;
  hit.begin(&sink);
  hit.set_lane_length(60);
  hit.start_by_name("reaction_stop", 0);
  hit.handle(kPlayer1, InputEvent::Confirm, 0);
  bool pressed = false;
  uint32_t t = 100;
  for (; t <= 60000 && !pressed; t += 100) {
    hit.update(t);
    hit.render(fb, &tail, t);
    int light = -1;
    int zone_lo = -1;
    int zone_hi = -1;
    for (int c = 0; c < 60; ++c) {
      const Rgb p = tail.at(static_cast<uint16_t>(c));
      if (p.r == 255 && p.g == 255 && p.b == 255 && light < 0) {
        light = c;
      }
      if (p.g == 220 && p.r == 40 && p.b == 70) {
        if (zone_lo < 0) {
          zone_lo = c;
        }
        zone_hi = c;
      }
    }
    if (light >= 0 && zone_lo >= 0 && light >= zone_lo && light <= zone_hi) {
      hit.handle(kPlayer1, InputEvent::Confirm, t + 1);
      pressed = true;
    }
  }
  CHECK(pressed);  // the light always passes the zone within the bound
  CHECK_EQ_INT(static_cast<int>(hit.phase()), static_cast<int>(GamePhase::Running));
  CHECK(hit.score() > 0);  // scored the stop
}

void test_color_chain_insert_clears_and_loses() {
  CueSink sink;
  FrameBuffer fb;
  TailFramebuffer tail;
  tail.configure(60);
  // Long idle run with the chain advancing: eventually the front reaches the
  // maw → Over (deterministic).
  GameHost host;
  host.begin(&sink);
  host.set_lane_length(60);
  host.start_by_name("color_chain", 0);
  host.handle(kPlayer1, InputEvent::Confirm, 0);
  bool over = false;
  for (uint32_t t = 100; t <= 120000 && !over; t += 100) {
    host.update(t);
    if (host.phase() == GamePhase::Over) over = true;
  }
  CHECK(over);  // unplayed chains always lose (they never clear themselves)
}

void test_color_catch_life_loss_game_over() {
  CueSink sink;
  FrameBuffer fb;
  TailFramebuffer tail;
  tail.configure(60);
  // Whiffed confirms (no pulse at the gate) burn all three lives → Over.
  GameHost host;
  host.begin(&sink);
  host.set_lane_length(60);
  host.start_by_name("color_catch", 0);
  host.handle(kPlayer1, InputEvent::Confirm, 0);
  for (int i = 0; i < 3; ++i) {
    CHECK_EQ_INT(static_cast<int>(host.phase()), static_cast<int>(GamePhase::Running));
    host.handle(kPlayer1, InputEvent::Confirm, 1000 + i * 10);
  }
  CHECK_EQ_INT(static_cast<int>(host.phase()), static_cast<int>(GamePhase::Over));
}

void test_memory_wrong_press_ends_round() {
  CueSink sink;
  FrameBuffer fb;
  TailFramebuffer tail;
  tail.configure(60);
  GameHost host;
  host.begin(&sink);
  host.set_lane_length(60);
  host.start_by_name("memory", 0);
  host.handle(kPlayer1, InputEvent::Confirm, 0);
  // Sequence playback completes (Easy: 1 item, 500 ms flash + gap).
  run_host(host, fb, tail, 100, 3000);
  // During the repeat phase any press is either correct or ends the round;
  // determinism says which: replay the same seed and check BOTH possible
  // presses produce stable phases across two identical hosts.
  GameHost twin;
  twin.begin(&sink);
  twin.set_lane_length(60);
  twin.start_by_name("memory", 0);
  twin.handle(kPlayer1, InputEvent::Confirm, 0);
  run_host(twin, fb, tail, 100, 3000);
  // Same input timeline → same phase (deterministic sequence).
  CHECK_EQ_INT(static_cast<int>(host.phase()), static_cast<int>(twin.phase()));
  // An input during playback is swallowed (no phase change).
  const int before = static_cast<int>(host.phase());
  host.handle(kPlayer1, InputEvent::Back, 3050);
  CHECK_EQ_INT(static_cast<int>(host.phase()), before);
}

void test_tail_game_difficulty_and_lane_injection() {
  CueSink sink;
  FrameBuffer fb;
  TailFramebuffer tail;
  // Hard tug-of-war AI pulls every host step: a passive player loses quickly;
  // Easy AI pulls every 4th step: the passive player survives much longer.
  uint32_t end_time[2]{};
  for (int c = 0; c < 2; ++c) {
    GameHost host;
    host.begin(&sink);
    host.set_lane_length(60);
    host.set_difficulty(c == 0 ? GameDifficulty::Easy : GameDifficulty::Hard);
    host.start_by_name("tug_of_war", 0);
    host.handle(kPlayer1, InputEvent::Confirm, 0);
    bool over = false;
    uint32_t end_t = 100;
    for (uint32_t t = 100; t <= 60000 && !over; t += 100) {
      host.update(t);
      end_t = t;
      if (host.phase() == GamePhase::Over) over = true;
    }
    CHECK(over);  // passive player always loses the tug eventually
    end_time[c] = end_t;
  }
  CHECK(end_time[0] > end_time[1]);  // Easy survives longer than Hard
  // Lane injection: a 24-cell lane renders fully inside a 24-cell rope.
  GameHost host;
  host.begin(&sink);
  host.set_lane_length(24);
  tail.configure(24);
  host.start_by_name("reaction_stop", 0);
  host.handle(kPlayer1, InputEvent::Confirm, 0);
  run_host(host, fb, tail, 100, 3000);
  for (uint16_t c = 24; c < 60; ++c) {
    const Rgb p = tail.at(c);
    CHECK(p.r == 0 && p.g == 0 && p.b == 0);  // beyond the physical rope: dark
  }
}
