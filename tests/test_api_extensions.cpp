#include "test_framework.h"

#include <cstring>

#include "api/command.h"
#include "api/device_services.h"
#include "games/game.h"
#include "input/input_events.h"
#include "menu/local_menu.h"
#include "render/framebuffer.h"
#include "sound/sound_sink.h"
#include "timer/countdown.h"
#include "visualizer/spectrum_input.h"

// LED-L009 dispatcher semantics for the additive capabilities, executed
// against a DeviceServices fake. Malformed commands mutate nothing and
// return typed errors; the machine transitions follow the documented model.

using namespace ledwall;

namespace {

class FakeServices final : public DeviceServices {
 public:
  // Timer passthrough into a real engine (behavioral fake).
  CountdownTimer timer;
  bool timer_set(uint32_t d) override { timer.arm(d); return true; }
  bool timer_start(uint32_t now) override { return timer.start(now); }
  bool timer_pause(uint32_t now) override { return timer.pause(now); }
  bool timer_resume(uint32_t now) override { return timer.resume(now); }
  void timer_reset(uint32_t now) override { timer.reset(now); }
  bool input_send(InputEvent ev) override { last_input = ev; ++inputs; return true; }
  bool menu_enter(uint32_t) override { menu_active = true; return true; }
  bool menu_exit() override { if (!menu_active) return false; menu_active = false; return true; }
  bool game_start(const char* n, uint32_t) override { game = n; return true; }
  bool game_command(const char* a, uint32_t) override { last_action = a; return true; }
  void game_exit() override { game = ""; }
  void game_set_difficulty(int d) override { last_difficulty = d; ++difficulty_sets; }
  bool visualizer_feed(const uint8_t* b, uint8_t c, uint32_t now) override {
    return spectrum.feed(b, c, now);
  }
  void visualizer_clear() override { spectrum.clear(); }
  void notify_store(const char* t, bool high) override {
    snprintf(notify_text, sizeof(notify_text), "%s", t);
    notify_high = high;
  }
  void play_cue(int cue) override { ++cues[cue]; }

  InputEvent last_input = InputEvent::Up;
  uint32_t inputs = 0;
  bool menu_active = false;
  const char* game = "";
  const char* last_action = "";
  int last_difficulty = -1;
  uint32_t difficulty_sets = 0;
  SpectrumInput spectrum;
  char notify_text[32] = {0};
  bool notify_high = false;
  int cues[12] = {};
};

struct Harness {
  ModeMachine machine;
  Brightness brightness;
  TextState text;
  EventSink events;
  FakeServices services;
  CommandDispatcher dispatcher;

  Harness() {
    machine.boot(ModeKind::Idle, 0);
    brightness.configure(40, 40);
    dispatcher.begin(&machine, &brightness, &text, &events);
    dispatcher.set_services(&services);
  }
  CommandResponse exec(SemanticCommand::Capability cap) {
    SemanticCommand c;
    c.capability = cap;
    return dispatcher.execute(c, 1000);
  }
};

}  // namespace

void test_v5_clear_exits_running_game() {
  Harness h;
  SemanticCommand start;
  start.capability = SemanticCommand::Capability::GameSet;
  std::strcpy(start.name, "pong");
  CHECK(h.dispatcher.execute(start, 1000).ok);
  CHECK_STR_EQ(h.services.game, "pong");
  CHECK(h.machine.snapshot(1000).base_mode == ModeKind::Game);
  SemanticCommand clear;
  clear.capability = SemanticCommand::Capability::Clear;
  CHECK(h.dispatcher.execute(clear, 1010).ok);
  CHECK_STR_EQ(h.services.game, "");
  CHECK(h.machine.snapshot(1010).base_mode == ModeKind::Idle);
}

void test_api_timer_semantics() {
  Harness h;
  // Start without a duration: typed error, no mode change.
  CommandResponse r = h.exec(SemanticCommand::Capability::TimerStart);
  CHECK(!r.ok);
  CHECK_STR_EQ(error_code_name(r.error), "invalidParameter");
  CHECK_EQ_INT(static_cast<int>(h.machine.snapshot(1000).base_mode),
               static_cast<int>(ModeKind::Idle));
  // Set + start: engine running, base becomes timer.
  SemanticCommand c;
  c.capability = SemanticCommand::Capability::TimerSet;
  c.has_duration = true;
  c.duration_ms = 10000;
  CHECK(h.dispatcher.execute(c, 1000).ok);
  CHECK(h.exec(SemanticCommand::Capability::TimerStart).ok);
  CHECK_EQ_INT(static_cast<int>(h.machine.snapshot(1000).base_mode),
               static_cast<int>(ModeKind::Timer));
  CHECK_EQ_INT(static_cast<int>(h.services.timer.state()),
               static_cast<int>(CountdownTimer::State::Running));
  // Pause / resume round-trip preserves exact ms.
  CHECK(h.exec(SemanticCommand::Capability::TimerPause).ok);
  CHECK_EQ_INT(static_cast<int>(h.services.timer.state()),
               static_cast<int>(CountdownTimer::State::Paused));
  CHECK(h.exec(SemanticCommand::Capability::TimerResume).ok);
  // Reset re-arms; stop leaves the timer display (base idle).
  CHECK(h.exec(SemanticCommand::Capability::TimerReset).ok);
  CHECK_EQ_INT(static_cast<int>(h.services.timer.state()),
               static_cast<int>(CountdownTimer::State::Idle));
  CHECK_EQ_INT(static_cast<int>(h.machine.snapshot(1000).base_mode),
               static_cast<int>(ModeKind::Timer));
  CHECK(h.exec(SemanticCommand::Capability::TimerStop).ok);
  CHECK_EQ_INT(static_cast<int>(h.machine.snapshot(1000).base_mode),
               static_cast<int>(ModeKind::Idle));
  // Invalid duration rejected with no mutation.
  SemanticCommand bad;
  bad.capability = SemanticCommand::Capability::TimerSet;
  bad.has_duration = true;
  bad.duration_ms = 0;
  CHECK(!h.dispatcher.execute(bad, 1000).ok);
}

void test_api_input_menu_games() {
  Harness h;
  SemanticCommand c;
  c.capability = SemanticCommand::Capability::InputSend;
  snprintf(c.input, sizeof(c.input), "%s", "confirm");
  c.has_input = true;
  CHECK(h.dispatcher.execute(c, 1000).ok);
  CHECK_EQ_INT(static_cast<int>(h.services.last_input), static_cast<int>(InputEvent::Confirm));
  snprintf(c.input, sizeof(c.input), "%s", "warp");
  CHECK(!h.dispatcher.execute(c, 1000).ok);
  // Menu enter/exit drives machine + service together.
  CHECK(h.exec(SemanticCommand::Capability::MenuSet).ok == false);  // no action: error
  SemanticCommand m;
  m.capability = SemanticCommand::Capability::MenuSet;
  snprintf(m.action, sizeof(m.action), "%s", "enter");
  m.has_action = true;
  CHECK(h.dispatcher.execute(m, 1000).ok);
  CHECK(h.services.menu_active);
  CHECK_EQ_INT(static_cast<int>(h.machine.snapshot(1000).base_mode),
               static_cast<int>(ModeKind::Menu));
  snprintf(m.action, sizeof(m.action), "%s", "exit");
  CHECK(h.dispatcher.execute(m, 1000).ok);
  CHECK(!h.services.menu_active);
  CHECK_EQ_INT(static_cast<int>(h.machine.snapshot(1000).base_mode),
               static_cast<int>(ModeKind::Idle));
  // Game set: typed unknownGame with the available list; valid start sets base.
  SemanticCommand g;
  g.capability = SemanticCommand::Capability::GameSet;
  snprintf(g.name, sizeof(g.name), "%s", "tetris");
  CommandResponse r = h.dispatcher.execute(g, 1000);
  CHECK(!r.ok);
  CHECK_STR_EQ(error_code_name(r.error), "unknownGame");
  CHECK(r.available[0] != '\0');
  snprintf(g.name, sizeof(g.name), "%s", "snake");
  CHECK(h.dispatcher.execute(g, 1000).ok);
  CHECK_STR_EQ(h.services.game, "snake");
  CHECK_EQ_INT(static_cast<int>(h.machine.snapshot(1000).base_mode),
               static_cast<int>(ModeKind::Game));
  SemanticCommand gc;
  gc.capability = SemanticCommand::Capability::GameCommand;
  snprintf(gc.action, sizeof(gc.action), "%s", "pause");
  gc.has_action = true;
  CHECK(h.dispatcher.execute(gc, 1000).ok);
  CHECK_STR_EQ(h.services.last_action, "pause");
  snprintf(gc.action, sizeof(gc.action), "%s", "explode");
  CHECK(!h.dispatcher.execute(gc, 1000).ok);
  snprintf(gc.action, sizeof(gc.action), "%s", "exit");
  CHECK(h.dispatcher.execute(gc, 1000).ok);
  CHECK_EQ_INT(static_cast<int>(h.machine.snapshot(1000).base_mode),
               static_cast<int>(ModeKind::Idle));
}

void test_api_visualizer_notify_standby() {
  Harness h;
  // Visualizer start/stop drive the machine mode.
  CHECK(h.exec(SemanticCommand::Capability::VisualizerStart).ok);
  CHECK_EQ_INT(static_cast<int>(h.machine.snapshot(1000).base_mode),
               static_cast<int>(ModeKind::Spectrum));
  // Data feed: shape + bounds validated at the transport; service accepts.
  SemanticCommand d;
  d.capability = SemanticCommand::Capability::VisualizerData;
  for (uint8_t i = 0; i < kSpectrumBands; ++i) d.bands[i] = 100;
  CHECK(h.dispatcher.execute(d, 1000).ok);
  CHECK(h.services.spectrum.active(1000));
  // Feed again too soon: typed invalidParameter (rate limit), no mutation.
  CHECK(!h.dispatcher.execute(d, 1010).ok);
  // Stop clears and leaves the mode.
  CHECK(h.exec(SemanticCommand::Capability::VisualizerStop).ok);
  CHECK_EQ_INT(static_cast<int>(h.machine.snapshot(1000).base_mode),
               static_cast<int>(ModeKind::Idle));
  CHECK(!h.services.spectrum.active(1000));
  // Notify: transient notification + stored payload; high priority recorded.
  SemanticCommand n;
  n.capability = SemanticCommand::Capability::Notify;
  snprintf(n.text, sizeof(n.text), "%s", "Dinner ready");
  n.has_duration = true;
  n.duration_ms = 4000;
  snprintf(n.priority, sizeof(n.priority), "%s", "high");
  n.has_priority = true;
  CHECK(h.dispatcher.execute(n, 1000).ok);
  CHECK(h.services.notify_high);
  CHECK_STR_EQ(h.services.notify_text, "Dinner ready");
  ModeSnapshot s = h.machine.snapshot(2000);
  CHECK(s.transient_active);
  CHECK_EQ_INT(static_cast<int>(s.transient_kind), static_cast<int>(ModeKind::Notification));
  // Empty notification text: invalidParameter, no transient.
  SemanticCommand bad;
  bad.capability = SemanticCommand::Capability::Notify;
  CHECK(!h.dispatcher.execute(bad, 1000).ok);
  // Standby: on → base standby (cancels the transient per §11); off → wake.
  CHECK(h.exec(SemanticCommand::Capability::StandbySet).ok == false);  // missing on
  SemanticCommand sb;
  sb.capability = SemanticCommand::Capability::StandbySet;
  sb.has_on = true;
  sb.on = true;
  CHECK(h.dispatcher.execute(sb, 1000).ok);
  CHECK_EQ_INT(static_cast<int>(h.machine.snapshot(1000).base_mode),
               static_cast<int>(ModeKind::Standby));
  // Brightness does NOT wake standby (§21 model).
  SemanticCommand br;
  br.capability = SemanticCommand::Capability::BrightnessSet;
  br.has_level = true;
  br.level = 30;
  CHECK(h.dispatcher.execute(br, 1000).ok);
  CHECK_EQ_INT(static_cast<int>(h.machine.snapshot(1000).base_mode),
               static_cast<int>(ModeKind::Standby));
  sb.on = false;
  CHECK(h.dispatcher.execute(sb, 1000).ok);
  CHECK_EQ_INT(static_cast<int>(h.machine.snapshot(1000).base_mode),
               static_cast<int>(ModeKind::Idle));
}

void test_api_text_studio_command_semantics() {
  Harness h;
  // Marquee RTL default via mode field; any width accepted.
  SemanticCommand c;
  c.capability = SemanticCommand::Capability::TextShow;
  snprintf(c.text, sizeof(c.text), "%s", "UNE JAM REDI NGA SHQIPERIA");
  c.has_mode = true;
  c.mode = TextMode::Marquee;
  CommandResponse r = h.dispatcher.execute(c, 1000);
  CHECK(r.ok);
  CHECK(h.text.base.scrolling);
  CHECK(h.text.base.rtl);       // explicit marquee defaults to the reading direction
  CHECK_EQ_INT(static_cast<int>(h.text.base.mode), static_cast<int>(TextMode::Marquee));
  // Legacy scroll flag keeps the heritage LTR direction.
  Harness h2;
  SemanticCommand s2;
  s2.capability = SemanticCommand::Capability::TextShow;
  snprintf(s2.text, sizeof(s2.text), "%s", "HERITAGE");
  s2.has_scroll = true;
  s2.scroll = true;
  CHECK(h2.dispatcher.execute(s2, 1000).ok);
  CHECK(!h2.text.base.rtl);
  CHECK_EQ_INT(static_cast<int>(h2.text.base.mode), static_cast<int>(TextMode::Marquee));
  // Fixed + too wide still rejects with textTooWide, no mutation.
  Harness h3;
  SemanticCommand s3;
  s3.capability = SemanticCommand::Capability::TextShow;
  snprintf(s3.text, sizeof(s3.text), "%s", "TOOLONGTEXT");
  CHECK(!h3.dispatcher.execute(s3, 1000).ok);
  CommandResponse err = h3.dispatcher.execute(s3, 1000);
  CHECK_STR_EQ(error_code_name(err.error), "textTooWide");
  // Words with dwell + loop.
  Harness h4;
  SemanticCommand s4;
  s4.capability = SemanticCommand::Capability::TextShow;
  snprintf(s4.text, sizeof(s4.text), "%s", "UNE JAM REDI");
  s4.has_mode = true;
  s4.mode = TextMode::Words;
  s4.has_word_dwell = true;
  s4.word_dwell_ms = 1500;
  s4.has_loop = true;
  s4.loop = false;
  CHECK(h4.dispatcher.execute(s4, 1000).ok);
  CHECK_EQ_INT(static_cast<int>(h4.text.base.mode), static_cast<int>(TextMode::Words));
  CHECK_EQ_INT(h4.text.base.word_dwell_ms, 1500);
  CHECK(!h4.text.base.loop);
  // Style + palette names resolve through the dispatcher's caller (transport);
  // invalid bounds rejected here.
  Harness h5;
  SemanticCommand s5;
  s5.capability = SemanticCommand::Capability::TextShow;
  snprintf(s5.text, sizeof(s5.text), "%s", "OK");
  s5.has_word_dwell = true;
  s5.word_dwell_ms = 100;  // below 200
  CHECK(!h5.dispatcher.execute(s5, 1000).ok);
}

void test_api_extended_without_services_fails_closed() {
  ModeMachine machine;
  Brightness brightness;
  TextState text;
  EventSink events;
  machine.boot(ModeKind::Idle, 0);
  brightness.configure(40, 40);
  CommandDispatcher d;
  d.begin(&machine, &brightness, &text, &events);  // no services
  SemanticCommand c;
  c.capability = SemanticCommand::Capability::TimerStart;
  CommandResponse r = d.execute(c, 0);
  CHECK(!r.ok);
  CHECK_STR_EQ(error_code_name(r.error), "internalError");
}

// ------------------------------------------------------------- V-L009-3 ----
void test_api_game_difficulty_semantics() {
  Harness h;
  // Valid names reach the services; the device owns the level.
  SemanticCommand ok;
  ok.capability = SemanticCommand::Capability::GameDifficulty;
  snprintf(ok.difficulty, sizeof(ok.difficulty), "%s", "hard");
  ok.has_difficulty = true;
  CommandResponse r = h.dispatcher.execute(ok, 1000);
  CHECK(r.ok);
  CHECK_EQ_INT(h.services.last_difficulty, static_cast<int>(games::GameDifficulty::Hard));

  // Missing field and unknown name reject with invalidParameter, no mutation.
  SemanticCommand missing;
  missing.capability = SemanticCommand::Capability::GameDifficulty;
  CommandResponse r2 = h.dispatcher.execute(missing, 1000);
  CHECK(!r2.ok);
  CHECK_STR_EQ(error_code_name(r2.error), "invalidParameter");

  SemanticCommand bad;
  bad.capability = SemanticCommand::Capability::GameDifficulty;
  snprintf(bad.difficulty, sizeof(bad.difficulty), "%s", "extreme");
  bad.has_difficulty = true;
  CommandResponse r3 = h.dispatcher.execute(bad, 1000);
  CHECK(!r3.ok);
  CHECK_STR_EQ(error_code_name(r3.error), "invalidParameter");
  CHECK_EQ_INT(h.services.difficulty_sets, 1);  // only the valid command mutated
}

void test_api_game_difficulty_without_services_fails_closed() {
  ModeMachine machine;
  Brightness brightness;
  TextState text;
  EventSink events;
  machine.boot(ModeKind::Idle, 0);
  brightness.configure(40, 40);
  CommandDispatcher d;
  d.begin(&machine, &brightness, &text, &events);  // no services
  SemanticCommand c;
  c.capability = SemanticCommand::Capability::GameDifficulty;
  snprintf(c.difficulty, sizeof(c.difficulty), "%s", "easy");
  c.has_difficulty = true;
  CommandResponse r = d.execute(c, 0);
  CHECK(!r.ok);
  CHECK_STR_EQ(error_code_name(r.error), "internalError");
}
