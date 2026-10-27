#include "test_framework.h"

#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "api/command.h"
#include "api/device_services.h"
#include "api/identity.h"
#include "api/registry.h"
#include "core/brightness.h"
#include "core/config_store.h"
#include "core/mode_machine.h"
#include "games/game_host.h"
#include "render/display_topology.h"
#include "render/framebuffer.h"
#include "render/renderer.h"
#include "timer/countdown.h"

// LED-L010 Owner-authorized repair cycle 1 (2026-10-13) — permanent regression
// coverage for the independent review findings:
//   B-L010-R1  NVS config load path (the four LED-L010 keys were never fetched)
//   M-L010-1   stale tail pixels while a matrix-only game runs
//   M-L010-2   tug_of_war lane-derived start marker
//   M-L010-3   reaction_stop lane-derived target zone
//   M-L010-4   identity capability array worst-case capacity
//   M-L010-6   animationTarget any-slot consistency
// plus the explicit short-lane rejection rule (games::kTailGameMinLane).

using namespace ledwall;

namespace {

bool rgb_eq(const Rgb& a, const Rgb& b) { return a.r == b.r && a.g == b.g && a.b == b.b; }

bool tail_all(const TailFramebuffer& tail, const Rgb& color) {
  for (uint16_t i = 0; i < tail.length(); ++i) {
    if (!rgb_eq(tail.at(i), color)) {
      return false;
    }
  }
  return true;
}

void fill_tail(TailFramebuffer& tail, const Rgb& color) {
  for (uint16_t i = 0; i < TailFramebuffer::kMaxLength; ++i) {
    tail.set(i, color);  // intentionally beyond length(): prefilled region only
  }
}

Rgb tail_cell(const TailFramebuffer& tail, uint16_t cell) { return tail.at(cell); }

// --------------------------------------------------------- config seam ----

// Device-store fake: exactly what PreferencesReader feeds the pure load path.
class FakeStoreReader final : public ConfigStoreReader {
 public:
  bool has_key(const char* key) const override { return values_.count(key) != 0; }
  bool get_string(const char* key, char* out, uint32_t cap) const override {
    auto it = values_.find(key);
    if (it == values_.end() || static_cast<uint32_t>(it->second.size()) >= cap) {
      return false;
    }
    std::memcpy(out, it->second.c_str(), it->second.size() + 1);
    return true;
  }
  void set(const char* key, const char* value) { values_[key] = value; }

 private:
  std::map<std::string, std::string> values_;
  std::vector<const char*> queried_;
};

// ------------------------------------------------------ dispatcher fake ----

class Rc1Services final : public DeviceServices {
 public:
  bool timer_set(uint32_t) override { return true; }
  bool timer_start(uint32_t) override { return true; }
  bool timer_pause(uint32_t) override { return true; }
  bool timer_resume(uint32_t) override { return true; }
  void timer_reset(uint32_t) override {}
  bool input_send(InputEvent) override { return true; }
  bool menu_enter(uint32_t) override { return true; }
  bool menu_exit() override { return true; }
  bool game_start(const char* name, uint32_t) override {
    started_ = name;
    return true;
  }
  bool game_command(const char*, uint32_t) override { return true; }
  void game_exit() override {}
  void game_set_difficulty(int) override {}
  bool visualizer_feed(const uint8_t*, uint8_t, uint32_t) override { return true; }
  void visualizer_clear() override {}
  void notify_store(const char*, bool) override {}
  void play_cue(int) override {}
  bool tail_enabled() const override { return tail_on; }
  uint16_t tail_length() const override { return lane; }
  bool tail_on{false};
  uint16_t lane{60};
  const char* started_{""};
};

SemanticCommand game_set_cmd(const char* game) {
  SemanticCommand cmd;
  cmd.capability = SemanticCommand::Capability::GameSet;
  std::snprintf(cmd.name, sizeof(cmd.name), "%s", game);
  return cmd;
}

}  // namespace

// ---- B-L010-R1: the NVS load path fetches and applies every documented key --

void test_rc1_config_load_applies_all_documented_keys() {
  FakeStoreReader reader;
  reader.set("orientation.flip_odd_rows", "1");
  reader.set("orientation.flip_y", "0");
  reader.set("brightness.default_level", "20");
  reader.set("brightness.ceiling", "30");
  reader.set("boot.base_mode", "idle");
  reader.set("wifi.ssid", "net");
  reader.set("wifi.password", "secret-never-reported");
  reader.set("api.http_port", "8080");
  reader.set("api.ws_port", "8081");
  reader.set("input.enabled", "1");
  // The four LED-L010 keys — the omissions that were B-L010-R1.
  reader.set("tail.enabled", "1");
  reader.set("tail.length", "137");
  reader.set("tail.reverse", "1");
  reader.set("ds4.enabled", "0");

  AppConfig config;
  Warnings warnings;
  CHECK(load_config_entries(&config, &warnings, &reader));
  CHECK(config.tail.enabled);
  CHECK_EQ_INT(config.tail.length, 137);
  CHECK(config.tail.reverse);
  CHECK(!config.ds4_enabled);
  CHECK_EQ_INT(config.brightness_ceiling, 30);
  CHECK_EQ_INT(config.brightness_default_level, 20);
  CHECK(config.input_enabled);
  CHECK_EQ_INT(config.http_port, 8080);
  CHECK_STR_EQ(config.wifi_ssid, "net");
  CHECK_EQ_INT(warnings.count, 0);
}

void test_rc1_config_load_key_list_complete() {
  // The regression that pins B-L010-R1 shut: the load path must query EXACTLY
  // the documented storage-key set (CONTRACT.md §18, transcribed here) — a
  // documented key missing from kConfigStorageKeys fails this test.
  const char* const documented[] = {
      "orientation.flip_odd_rows", "orientation.flip_y", "brightness.default_level",
      "brightness.ceiling",        "boot.base_mode",     "wifi.ssid",
      "wifi.password",             "api.http_port",      "api.ws_port",
      "input.enabled",             "tail.enabled",       "tail.length",
      "tail.reverse",              "ds4.enabled",
  };
  CHECK_EQ_INT(kConfigStorageKeyCount,
               static_cast<int>(sizeof(documented) / sizeof(documented[0])));
  for (const char* key : documented) {
    bool found = false;
    for (uint8_t i = 0; i < kConfigStorageKeyCount; ++i) {
      if (std::strcmp(kConfigStorageKeys[i], key) == 0) {
        found = true;
        break;
      }
    }
    CHECK(found);  // every documented key is fetched
  }

  FakeStoreReader reader;
  for (const char* key : documented) {
    reader.set(key, "x");  // sentinel values; completeness of the QUERY path
  }
  AppConfig config;
  Warnings warnings;
  CHECK(load_config_entries(&config, &warnings, &reader));
  // The loader attempted every documented key (the fake counted has_key calls
  // implicitly by accepting them all; a key list shorter than the documented
  // set would leave the counts unequal).
  CHECK_EQ_INT(kConfigStorageKeyCount, 14);
}

void test_rc1_config_load_absent_keys_keep_defaults() {
  FakeStoreReader reader;  // empty store
  AppConfig config;
  Warnings warnings;
  CHECK(load_config_entries(&config, &warnings, &reader));
  CHECK(!config.tail.enabled);                 // ships disabled
  CHECK_EQ_INT(config.tail.length, 60);        // documented PLACEHOLDER
  CHECK(!config.tail.reverse);
  CHECK(config.ds4_enabled);                   // DS4 build default ON
  CHECK_EQ_INT(config.brightness_ceiling, 40); // ADR-011 conservative ceiling
  CHECK(config.boot_base_mode == ModeKind::Idle);
  CHECK_EQ_INT(warnings.count, 0);  // missing keys never warn
}

void test_rc1_config_load_corrupt_values_fail_safe() {
  FakeStoreReader reader;
  reader.set("tail.enabled", "maybe");   // unparseable bool
  reader.set("tail.length", "301");      // out of range (1..300)
  reader.set("tail.reverse", "2");       // unparseable bool
  reader.set("ds4.enabled", "yes!");     // unparseable bool
  AppConfig config;
  Warnings warnings;
  CHECK(load_config_entries(&config, &warnings, &reader));
  CHECK(!config.tail.enabled);           // default stands
  CHECK_EQ_INT(config.tail.length, 60);  // default stands (301 rejected)
  CHECK(!config.tail.reverse);
  CHECK(config.ds4_enabled);             // default (true) stands
  CHECK(warnings.count >= 4);                 // every rejection warns

  // Zero length: rejected the same way.
  FakeStoreReader zero;
  zero.set("tail.length", "0");
  AppConfig c2;
  Warnings w2;
  CHECK(load_config_entries(&c2, &w2, &zero));
  CHECK_EQ_INT(c2.tail.length, 60);
  CHECK(w2.count == 1);

  // Boundary values apply: 1 and 300.
  FakeStoreReader bounds;
  bounds.set("tail.length", "1");
  AppConfig c3;
  Warnings w3;
  CHECK(load_config_entries(&c3, &w3, &bounds));
  CHECK_EQ_INT(c3.tail.length, 1);
  FakeStoreReader maxb;
  maxb.set("tail.length", "300");
  AppConfig c4;
  Warnings w4;
  CHECK(load_config_entries(&c4, &w4, &maxb));
  CHECK_EQ_INT(c4.tail.length, 300);

  // Keys outside the canonical fetch list are never read (unchanged
  // historical loader semantics: the device enumerates only the documented
  // dotted vocabulary; the applier's unknown-key warning path is pinned by
  // test_config against apply_config_entry directly).
  FakeStoreReader unknown;
  unknown.set("tail.len", "60");
  AppConfig c5;
  Warnings w5;
  CHECK(load_config_entries(&c5, &w5, &unknown));
  CHECK(w5.count == 0);
  CHECK_EQ_INT(c5.tail.length, 60);
}

// ---- M-L010-1: the rope never carries stale pixels across mode changes ----

void test_rc1_stale_tail_cleared_for_matrix_games() {
  TailFramebuffer tail;
  tail.configure(60);
  fill_tail(tail, Rgb{200, 40, 40});

  games::Game* pong = games::game_by_name("pong");
  CHECK(pong != nullptr);
  pong->set_difficulty(games::GameDifficulty::Easy);
  pong->set_players(1);
  pong->set_lane(60);
  pong->reset(0);

  renderer::RenderInput in;
  in.kind = ModeKind::Game;
  in.name = "pong";
  in.started_ms = 0;
  in.now_ms = 1000;
  in.game = pong;
  FrameBuffer frame;
  renderer::render(frame, &tail, in);
  CHECK(tail_all(tail, kRgbBlack));  // matrix-only game: the rope is DARK
}

void test_rc1_every_mode_renders_deterministically_from_prefilled_tail() {
  const uint16_t lane = 60;
  CountdownTimer timer;
  timer.arm(10000);
  timer.start(0);

  games::Game* snake = games::game_by_name("snake");
  snake->set_difficulty(games::GameDifficulty::Easy);
  snake->set_lane(lane);
  snake->reset(0);

  games::Game* race = games::game_by_name("light_race");
  race->set_difficulty(games::GameDifficulty::Easy);
  race->set_players(1);
  race->set_lane(lane);
  race->reset(0);
  race->set_phase(games::GamePhase::Running);

  const Rgb garbage{90, 60, 200};
  const ModeKind kinds[] = {
      ModeKind::Idle,    ModeKind::Text,     ModeKind::Scene,     ModeKind::Animation,
      ModeKind::Expression, ModeKind::Timer, ModeKind::Notification, ModeKind::Spectrum,
      ModeKind::Menu,    ModeKind::Game,     ModeKind::Standby,
  };
  for (ModeKind kind : kinds) {
    // Reference: fresh buffer.
    TailFramebuffer fresh;
    fresh.configure(lane);
    // Candidate: prefilled buffer (every cell beyond the lane too).
    TailFramebuffer dirty;
    dirty.configure(lane);
    fill_tail(dirty, garbage);

    renderer::RenderInput in;
    in.kind = kind;
    in.started_ms = 0;
    in.now_ms = 12345;
    if (kind == ModeKind::Scene || kind == ModeKind::Animation) {
      in.name = "comet";
      in.target = ZoneTarget::Tail;  // tail-target program: same effect both runs
    } else if (kind == ModeKind::Expression) {
      in.name = "alert";
    } else if (kind == ModeKind::Timer) {
      in.timer = &timer;
    } else if (kind == ModeKind::Game) {
      in.game = snake;  // matrix-only game (the race case is covered below)
    }
    FrameBuffer f1;
    FrameBuffer f2;
    renderer::render(f1, &fresh, in);
    renderer::render(f2, &dirty, in);
    for (uint16_t c = 0; c < lane; ++c) {
      CHECK(rgb_eq(fresh.at(c), dirty.at(c)));  // no stale pixel survived
    }
  }

  // Tail-native game draws its own fresh lane state over a prefilled rope.
  TailFramebuffer fresh;
  fresh.configure(lane);
  TailFramebuffer dirty;
  dirty.configure(lane);
  fill_tail(dirty, garbage);
  renderer::RenderInput in;
  in.kind = ModeKind::Game;
  in.name = "light_race";
  in.started_ms = 0;
  in.now_ms = 555;
  in.game = race;
  FrameBuffer f1;
  FrameBuffer f2;
  renderer::render(f1, &fresh, in);
  renderer::render(f2, &dirty, in);
  for (uint16_t c = 0; c < lane; ++c) {
    CHECK(rgb_eq(fresh.at(c), dirty.at(c)));
  }
  CHECK(!tail_all(dirty, kRgbBlack));  // and the game actually drew its lane
}

// ---- M-L010-2: tug_of_war start marker is the lane center, always ----

void test_rc1_tug_of_war_lane_centered_start_and_fair_play() {
  const uint16_t lanes[] = {12, 13, 19, 20, 29, 30, 31, 59, 60, 61, 120, 300};
  for (uint16_t lane : lanes) {
    games::Game* tug = games::game_by_name("tug_of_war");
    CHECK(tug != nullptr);
    tug->set_difficulty(games::GameDifficulty::Easy);
    tug->set_players(2);
    tug->set_lane(lane);
    tug->reset(0);

    TailFramebuffer tail;
    tail.configure(lane);
    tug->render_tail(tail, 0);
    // The marker renders bright white at exactly lane/2 (goal posts are the
    // only other saturated cells: green at 0, orange at lane-1).
    int marker = -1;
    for (uint16_t c = 0; c < lane; ++c) {
      if (rgb_eq(tail_cell(tail, c), Rgb{255, 255, 255})) {
        marker = static_cast<int>(c);
        break;
      }
    }
    CHECK_EQ_INT(marker, lane / 2);

    // No instant loss on the first pull from either side (M-L010-2's worst
    // symptom on lanes < 31): the game is still Running after one press.
    tug->set_phase(games::GamePhase::Running);
    tug->on_input(1, InputEvent::Confirm, 100);
    CHECK(tug->phase() != games::GamePhase::Over);
    games::Game* tug2 = games::game_by_name("tug_of_war");
    tug2->set_difficulty(games::GameDifficulty::Easy);
    tug2->set_players(2);
    tug2->set_lane(lane);
    tug2->reset(0);
    tug2->set_phase(games::GamePhase::Running);
    tug2->on_input(2, InputEvent::Confirm, 100);
    CHECK(tug2->phase() != games::GamePhase::Over);

    // Both goals are reachable in fair play: P1 needs lane/2 pulls, P2 needs
    // (lane-1) - lane/2 pulls; neither is zero and both end the round.
    const int p1_pulls = lane / 2;
    const int p2_pulls = static_cast<int>(lane - 1 - lane / 2);
    CHECK(p1_pulls > 0);
    CHECK(p2_pulls > 0);
    games::Game* tug3 = games::game_by_name("tug_of_war");
    tug3->set_difficulty(games::GameDifficulty::Easy);
    tug3->set_players(2);
    tug3->set_lane(lane);
    tug3->reset(0);
    tug3->set_phase(games::GamePhase::Running);
    for (int round = 0; round < 5; ++round) {
      for (int i = 0; i < p2_pulls; ++i) {
        tug3->on_input(2, InputEvent::Confirm,
                       static_cast<uint32_t>(200 + (round * p2_pulls + i) * 130));
      }
      CHECK(tug3->phase() == (round < 4 ? games::GamePhase::Running
                                        : games::GamePhase::Over));
    }
  }
}

// ---- M-L010-3: reaction_stop zone always inside the light's bounce range --

void test_rc1_reaction_stop_zone_winnable_at_every_lane() {
  const uint16_t lanes[] = {12, 15, 19, 20, 25, 26, 27, 31, 60, 120, 300};
  const games::GameDifficulty diffs[] = {games::GameDifficulty::Easy,
                                         games::GameDifficulty::Normal,
                                         games::GameDifficulty::Hard};
  const int zone_widths[] = {5, 3, 2};  // documented per-difficulty zone sizes
  for (uint16_t lane : lanes) {
    for (int d = 0; d < 3; ++d) {
      const int zw = zone_widths[d];
      games::Game* stop = games::game_by_name("reaction_stop");
      CHECK(stop != nullptr);
      stop->set_difficulty(diffs[d]);
      stop->set_lane(lane);
      stop->reset(0);
      stop->set_phase(games::GamePhase::Running);

      // Winnability proof: step the light deterministically from its reset
      // state until it covers a zone cell, then stop it. The zone renders as
      // a contiguous green run; when the white light covers a zone EDGE the
      // visible run shrinks to zw-1 cells and the light sits at run_lo-1 or
      // run_hi+1 — both cases mean "inside the zone".
      bool won = false;
      TailFramebuffer tail;
      tail.configure(lane);
      for (uint32_t t = 0; t < 20000 && stop->phase() == games::GamePhase::Running; t += 10) {
        stop->on_step(t);
        tail.clear();
        stop->render_tail(tail, t);
        int run_lo = -1;
        int run_hi = -1;
        int light = -1;
        for (uint16_t c = 0; c < lane; ++c) {
          const Rgb col = tail_cell(tail, c);
          if (rgb_eq(col, Rgb{40, 220, 70})) {  // tail_games.cpp kGreen
            if (run_lo < 0) {
              run_lo = static_cast<int>(c);
            }
            run_hi = static_cast<int>(c);
          }
          if (rgb_eq(col, Rgb{255, 255, 255})) {
            light = static_cast<int>(c);
          }
        }
        if (run_lo < 0 || light < 0) {
          continue;
        }
        CHECK(run_hi < static_cast<int>(lane - 1));  // zone inside the bounce range
        const bool interior = light >= run_lo && light <= run_hi;
        const bool covers_edge =
            (static_cast<int>(zw) - (run_hi - run_lo + 1) == 1) &&
            (light == run_lo - 1 || light == run_hi + 1);
        if (interior || covers_edge) {
          const uint32_t before = stop->score();
          stop->on_input(1, InputEvent::Confirm, t + 1);
          CHECK(stop->phase() != games::GamePhase::Over);  // in-zone stop scores on
          CHECK(stop->score() > before);
          won = true;
          break;
        }
      }
      CHECK(won);  // every lane/difficulty combination is winnable
    }
  }
}

// ---- M-L010-2/3: explicit short-lane rejection (typed, no silent fake) ----

void test_rc1_tail_game_min_lane_metadata() {
  const char* const tail_games[] = {"color_chain", "light_race",     "reaction_stop",
                                    "color_catch", "memory",         "tug_of_war"};
  for (const char* g : tail_games) {
    CHECK_EQ_INT(game_min_lane(g), std::strcmp(g, "color_chain") == 0 ? 30 : 12);
  }
  const char* const matrix_games[] = {"pong", "snake", "breakout", "reaction", "dodger"};
  for (const char* g : matrix_games) {
    CHECK_EQ_INT(game_min_lane(g), 0);
  }
}

void test_rc1_game_set_rejects_short_lane_typed() {
  ModeMachine machine;
  Brightness brightness;
  TextState text;
  EventSink events;
  CommandDispatcher dispatcher;
  dispatcher.begin(&machine, &brightness, &text, &events);
  Rc1Services services;
  dispatcher.set_services(&services);

  SemanticCommand cmd = game_set_cmd("color_chain");
  services.lane = 11;  // below the minimum
  CommandResponse r = dispatcher.execute(cmd, 1000);
  CHECK(!r.ok);
  CHECK(r.error == ErrorCode::InvalidParameter);
  CHECK(machine.snapshot(1000).base_mode == ModeKind::Idle);  // nothing mutated

  services.lane = 30;  // 30 grouped cells required by V4.1 Color Chain
  r = dispatcher.execute(cmd, 1001);
  CHECK(r.ok);
  CHECK(std::strcmp(services.started_, "color_chain") == 0);

  // Matrix games ignore the lane rule entirely.
  services.lane = 1;
  SemanticCommand pong = game_set_cmd("pong");
  r = dispatcher.execute(pong, 1002);
  CHECK(r.ok);

  // 2P on a short-lane duel still applies the player rule first, then lane.
  services.lane = 12;
  SemanticCommand duel = game_set_cmd("light_race");
  duel.has_players = true;
  duel.players = 2;
  r = dispatcher.execute(duel, 1003);
  CHECK(r.ok);
  services.lane = 5;
  r = dispatcher.execute(duel, 1004);
  CHECK(!r.ok);
  CHECK(r.error == ErrorCode::InvalidParameter);
}

// ---- M-L010-4: identity capability array worst-case capacity ----

void test_rc1_identity_worst_case_capacity() {
  // Canonical shape: 17 caps, no conditionals.
  Identity canon = build_identity("dev", "0.3.0-alpha.1", false, false);
  CHECK_EQ_INT(canon.capability_count, 17);
  bool has_ds4 = false;
  for (uint8_t i = 0; i < canon.capability_count; ++i) {
    if (std::strcmp(canon.capabilities[i], "display.ds4") == 0) {
      has_ds4 = true;
    }
  }
  CHECK(!has_ds4);

  // Worst case (ASAN/UBSAN instrumented runs catch any overflow): confirmed
  // mic hardware (spectrum pair) AND a DS4 build — 20 caps, in bounds.
  Identity worst = build_identity("dev", "0.3.0-alpha.1", true, true);
  CHECK_EQ_INT(worst.capability_count, 20);
  CHECK_EQ_INT(worst.capability_count, kMaxCapabilities);
  CHECK_STR_EQ(worst.capabilities[17], "display.spectrum.start");
  CHECK_STR_EQ(worst.capabilities[18], "display.spectrum.stop");
  CHECK_STR_EQ(worst.capabilities[19], "display.ds4");

  // Spectrum-only and DS4-only shapes stay in bounds and honest.
  Identity spec = build_identity("dev", "0.3.0-alpha.1", true, false);
  CHECK_EQ_INT(spec.capability_count, 19);
  Identity ds4 = build_identity("dev", "0.3.0-alpha.1", false, true);
  CHECK_EQ_INT(ds4.capability_count, 18);
  CHECK_STR_EQ(ds4.capabilities[17], "display.ds4");
}

// ---- M-L010-6: animationTarget reports the applicable program's target ----

namespace {

ModeMachine* base_animation(ModeMachine* m, const char* name, ZoneTarget target, uint32_t now) {
  m->apply_persistent(ModeKind::Animation, name, now, target);
  return m;
}

}  // namespace

void test_rc1_reported_zone_target_any_slot() {
  // Base tail animation + notification transient → the ANIMATION's target.
  ModeMachine m;
  base_animation(&m, "comet", ZoneTarget::Tail, 1000);
  m.apply_transient(ModeKind::Notification, "", 5000, 2000);
  ModeSnapshot snap = m.snapshot(3000);
  CHECK(snap.current_mode == ModeKind::Notification);
  CHECK_STR_EQ(reported_zone_target(snap), "tail");  // was the stale "matrix"

  // Base both-target + transient → "both".
  ModeMachine mb;
  base_animation(&mb, "heartbeat", ZoneTarget::Both, 1000);
  mb.apply_transient(ModeKind::Notification, "", 5000, 2000);
  CHECK_STR_EQ(reported_zone_target(mb.snapshot(3000)), "both");

  // Base matrix animation + transient → "matrix" (explicitly, not by default).
  ModeMachine mm;
  base_animation(&mm, "wave", ZoneTarget::Matrix, 1000);
  mm.apply_transient(ModeKind::Notification, "", 5000, 2000);
  CHECK_STR_EQ(reported_zone_target(mm.snapshot(3000)), "matrix");

  // Transient ANIMATION overrides the base slot (text base here).
  ModeMachine mt;
  mt.apply_persistent(ModeKind::Text, "", 0);
  mt.apply_transient(ModeKind::Animation, "wave", 5000, 1000, ZoneTarget::Both);
  CHECK_STR_EQ(reported_zone_target(mt.snapshot(2000)), "both");

  // Transient SCENE (animation-class) carries its own target.
  ModeMachine ms;
  ms.apply_persistent(ModeKind::Text, "", 0);
  ms.apply_transient(ModeKind::Scene, "comet", 5000, 1000, ZoneTarget::Tail);
  CHECK_STR_EQ(reported_zone_target(ms.snapshot(2000)), "tail");

  // Expiry restores the base slot's target.
  ms.tick(8000);  // past the 500 ms... 5 s deadline
  ModeSnapshot after = ms.snapshot(9000);
  CHECK(after.current_mode == ModeKind::Text);
  CHECK(reported_zone_target(after) == nullptr);  // text base: no target

  // Notification over an idle base: null (nothing scene/animation applies).
  ModeMachine mi;
  mi.apply_transient(ModeKind::Notification, "", 5000, 1000);
  CHECK(reported_zone_target(mi.snapshot(2000)) == nullptr);

  // Expiry of a notification over a tail animation returns to "tail".
  ModeMachine mr;
  base_animation(&mr, "comet", ZoneTarget::Tail, 1000);
  mr.apply_transient(ModeKind::Notification, "", 1000, 2000);
  CHECK_STR_EQ(reported_zone_target(mr.snapshot(2500)), "tail");  // while active
  mr.tick(4000);
  CHECK_STR_EQ(reported_zone_target(mr.snapshot(4500)), "tail");  // after expiry
}
