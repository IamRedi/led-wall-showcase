#include "test_framework.h"

#include <cstdio>
#include <cstring>

#include "api/command.h"
#include "api/device_services.h"
#include "core/brightness.h"
#include "core/mode_machine.h"
#include "games/game_host.h"
#include "input/input_events.h"
#include "sound/sound_sink.h"

// LED-L010 P1/P2 architecture at the semantic API (ADR-LED-L010 D4): optional
// player attribution on input.send, per-game player modes validated against
// registry metadata, and no cross-talk between the two slots.

using namespace ledwall;

namespace {

// Records services calls (the dispatcher decides, this observes).
class RecordingServices final : public DeviceServices {
 public:
  bool timer_set(uint32_t) override { return true; }
  bool timer_start(uint32_t) override { return true; }
  bool timer_pause(uint32_t) override { return true; }
  bool timer_resume(uint32_t) override { return true; }
  void timer_reset(uint32_t) override {}
  bool input_send(InputEvent) override { return true; }
  bool input_send_player(uint8_t player, InputEvent event) override {
    last_player = player;
    last_event = static_cast<int>(event);
    ++player_calls;
    return true;
  }
  bool menu_enter(uint32_t) override { return true; }
  bool menu_exit() override { return true; }
  bool game_start(const char*, uint32_t) override { return true; }
  bool game_command(const char*, uint32_t) override { return true; }
  void game_exit() override {}
  void game_set_difficulty(int) override {}
  void game_set_players(int players) override { last_players = players; }
  bool visualizer_feed(const uint8_t*, uint8_t, uint32_t) override { return true; }
  void visualizer_clear() override {}
  void notify_store(const char*, bool) override {}
  void play_cue(int) override {}
  bool tail_enabled() const override { return tail_enabled_; }

  int player_calls{0};
  uint8_t last_player{0};
  int last_event{-1};
  int last_players{-1};
  bool tail_enabled_{false};
};

}  // namespace

void test_players_api_player_attribution() {
  ModeMachine machine;
  Brightness brightness;
  TextState text;
  EventSink events;
  CommandDispatcher dispatcher;
  dispatcher.begin(&machine, &brightness, &text, &events);
  RecordingServices services;
  dispatcher.set_services(&services);

  SemanticCommand cmd;
  cmd.capability = SemanticCommand::Capability::InputSend;
  std::snprintf(cmd.input, sizeof(cmd.input), "confirm");
  cmd.has_input = true;
  // Absent player → P1 (legacy wire form unchanged).
  CommandResponse r = dispatcher.execute(cmd, 1000);
  CHECK(r.ok);
  CHECK_EQ_INT(services.last_player, kPlayer1);
  // Explicit player 2 → routed with the slot.
  cmd.has_player = true;
  cmd.player = 2;
  r = dispatcher.execute(cmd, 1001);
  CHECK(r.ok);
  CHECK_EQ_INT(services.last_player, kPlayer2);
  // Invalid player values reject without calling services.
  const int calls_before = services.player_calls;
  cmd.player = 3;
  r = dispatcher.execute(cmd, 1002);
  CHECK(!r.ok);
  CHECK(r.error == ErrorCode::InvalidParameter);
  cmd.player = 0;
  r = dispatcher.execute(cmd, 1003);
  CHECK(!r.ok);
  CHECK_EQ_INT(services.player_calls, calls_before);
}

void test_players_game_mode_validation() {
  ModeMachine machine;
  Brightness brightness;
  TextState text;
  EventSink events;
  CommandDispatcher dispatcher;
  dispatcher.begin(&machine, &brightness, &text, &events);
  RecordingServices services;
  dispatcher.set_services(&services);

  SemanticCommand cmd;
  cmd.capability = SemanticCommand::Capability::GameSet;
  std::snprintf(cmd.name, sizeof(cmd.name), "tug_of_war");
  // 2P on a 2P-capable game → accepted, engine told.
  cmd.has_players = true;
  cmd.players = 2;
  CommandResponse r = dispatcher.execute(cmd, 1000);
  CHECK(r.ok);
  CHECK_EQ_INT(services.last_players, 2);
  // Solo form.
  cmd.players = 1;
  r = dispatcher.execute(cmd, 1001);
  CHECK(r.ok);
  CHECK_EQ_INT(services.last_players, 1);
  // 2P on a solo-only game → invalidParameter, engine untouched.
  std::snprintf(cmd.name, sizeof(cmd.name), "snake");
  cmd.players = 2;
  r = dispatcher.execute(cmd, 1002);
  CHECK(!r.ok);
  CHECK(r.error == ErrorCode::InvalidParameter);
  // Out-of-range player count rejected.
  std::snprintf(cmd.name, sizeof(cmd.name), "pong");
  cmd.players = 5;
  r = dispatcher.execute(cmd, 1003);
  CHECK(!r.ok);
  CHECK(r.error == ErrorCode::InvalidParameter);
}

void test_players_pong_2p_no_cross_talk() {
  // Through the engine: a P2 up/down must move ONLY the right paddle.
  games::GameHost host;
  NullSoundSink sink;
  host.begin(&sink);
  host.set_players(2);
  host.start_by_name("pong", 0);
  host.handle(kPlayer1, InputEvent::Confirm, 0);  // → Running
  // Drive P2's paddle up five times; P1's must stay at rows 3..5.
  games::Game* pong = host.current_game();
  for (int i = 0; i < 5; ++i) {
    pong->on_input(kPlayer2, InputEvent::Up, 100 + i * 10);
  }
  FrameBuffer fb;
  pong->render(fb, 200);
  int left_lo = -1;
  int left_hi = -1;
  for (int y = 0; y < 10; ++y) {
    if (fb.at(1, static_cast<uint8_t>(y)) == Rgb{40, 200, 60}) {
      if (left_lo < 0) left_lo = y;
      left_hi = y;
    }
  }
  CHECK_EQ_INT(left_lo, 3);
  CHECK_EQ_INT(left_hi, 5);  // P1 untouched
  int right_lo = -1;
  for (int y = 0; y < 10; ++y) {
    if (fb.at(28, static_cast<uint8_t>(y)) != Rgb{0, 0, 0}) {
      right_lo = y;
      break;
    }
  }
  CHECK(right_lo < 3);  // moved up from the 3..5 start
}
