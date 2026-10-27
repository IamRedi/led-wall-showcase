#include "test_framework.h"

#include <cstdio>
#include <cstring>

#include "api/command.h"
#include "api/device_services.h"
#include "core/config.h"
#include "core/mode_machine.h"
#include "core/public_state.h"
#include "core/brightness.h"

// LED-L010 API semantics (ADR-LED-L010 D3/D8): zone targets on
// scene/animation.set (validation, defaults, ledUnavailable when the tail is
// disabled), zone-carrying mode snapshots, config keys, and the extended
// public-state views.

using namespace ledwall;

namespace {

class FakeServices final : public DeviceServices {
 public:
  bool timer_set(uint32_t) override { return true; }
  bool timer_start(uint32_t) override { return true; }
  bool timer_pause(uint32_t) override { return true; }
  bool timer_resume(uint32_t) override { return true; }
  void timer_reset(uint32_t) override {}
  bool input_send(InputEvent) override { return true; }
  bool menu_enter(uint32_t) override { return true; }
  bool menu_exit() override { return true; }
  bool game_start(const char*, uint32_t) override { return true; }
  bool game_command(const char*, uint32_t) override { return true; }
  void game_exit() override {}
  void game_set_difficulty(int) override {}
  bool visualizer_feed(const uint8_t*, uint8_t, uint32_t) override { return true; }
  void visualizer_clear() override {}
  void notify_store(const char*, bool) override {}
  void play_cue(int) override {}
  bool tail_enabled() const override { return tail_on; }
  uint16_t tail_length() const override { return 200; }
  bool tail_point_set(uint16_t index) override { point_index = static_cast<int16_t>(index); return true; }
  void tail_point_off() override { point_index = -1; }
  bool tail_point_active() const override { return point_index >= 0; }
  bool tail_on{false};
  int16_t point_index{-1};
};

}  // namespace

void test_l010_tail_setup_point_gate_and_off() {
  ModeMachine machine;
  Brightness brightness;
  brightness.configure(30, 40);
  TextState text;
  EventSink events;
  CommandDispatcher dispatcher;
  dispatcher.begin(&machine, &brightness, &text, &events);
  FakeServices services;
  dispatcher.set_services(&services);
  SemanticCommand cmd;
  cmd.capability = SemanticCommand::Capability::TailPoint;
  cmd.has_tail_index = true;
  cmd.tail_index = 170;
  CHECK(dispatcher.execute(cmd, 1000).error == ErrorCode::LedUnavailable);
  services.tail_on = true;
  CHECK(dispatcher.execute(cmd, 1001).ok);
  CHECK_EQ_INT(services.point_index, 170);
  CHECK_EQ_INT(brightness.level(), 5);
  cmd.tail_index = 200;
  CHECK(dispatcher.execute(cmd, 1002).error == ErrorCode::InvalidParameter);
  CHECK_EQ_INT(services.point_index, 170);  // rejected command mutates nothing
  cmd.tail_index = 199;
  CHECK(dispatcher.execute(cmd, 1003).ok);
  CHECK_EQ_INT(services.point_index, 199);
  SemanticCommand high;
  high.capability = SemanticCommand::Capability::BrightnessSet;
  high.has_level = true;
  high.level = 40;
  CHECK(dispatcher.execute(high, 1004).error == ErrorCode::InvalidParameter);
  CHECK_EQ_INT(brightness.level(), 5);
  SemanticCommand off;
  off.capability = SemanticCommand::Capability::TailPointOff;
  CHECK(dispatcher.execute(off, 1005).ok);
  CHECK_EQ_INT(services.point_index, -1);
  CHECK(machine.snapshot(1005).base_mode == ModeKind::Idle);
}

void test_l010_animation_target_defaults_and_validation() {
  ModeMachine machine;
  Brightness brightness;
  TextState text;
  EventSink events;
  CommandDispatcher dispatcher;
  dispatcher.begin(&machine, &brightness, &text, &events);
  FakeServices services;
  dispatcher.set_services(&services);

  // Default target for a tail-native animation is Tail — but the tail is
  // disabled → ledUnavailable (the reserved code, activated).
  SemanticCommand cmd;
  cmd.capability = SemanticCommand::Capability::AnimationSet;
  std::snprintf(cmd.name, sizeof(cmd.name), "comet");
  CommandResponse r = dispatcher.execute(cmd, 1000);
  CHECK(!r.ok);
  CHECK(r.error == ErrorCode::LedUnavailable);
  CHECK_STR_EQ(error_code_name(r.error), "ledUnavailable");
  // Nothing mutated.
  CHECK(machine.snapshot(1000).base_mode == ModeKind::Idle);

  // Matrix-target request for a matrix animation works with the tail off.
  std::snprintf(cmd.name, sizeof(cmd.name), "wave");
  r = dispatcher.execute(cmd, 1001);
  CHECK(r.ok);
  ModeSnapshot snap = machine.snapshot(1001);
  CHECK(snap.base_mode == ModeKind::Animation);
  CHECK_STR_EQ(snap.base_name, "wave");
  CHECK(snap.base_zone_target == ZoneTarget::Matrix);

  // Explicit target on the wire: unsupported combination with the tail OFF
  // reports the zone as unavailable (availability precedes the mask check —
  // the rope does not exist); with the tail on, the same request is a plain
  // invalidParameter (covered in the gate test below).
  std::snprintf(cmd.name, sizeof(cmd.name), "wave");
  cmd.has_target = true;
  std::snprintf(cmd.target, sizeof(cmd.target), "tail");
  r = dispatcher.execute(cmd, 1002);
  CHECK(!r.ok);
  CHECK(r.error == ErrorCode::LedUnavailable);

  // Invalid target string rejects.
  std::snprintf(cmd.target, sizeof(cmd.target), "rope");
  r = dispatcher.execute(cmd, 1003);
  CHECK(!r.ok);
  CHECK(r.error == ErrorCode::InvalidParameter);
}

void test_l010_target_carry_and_tail_enable_gate() {
  ModeMachine machine;
  Brightness brightness;
  TextState text;
  EventSink events;
  CommandDispatcher dispatcher;
  dispatcher.begin(&machine, &brightness, &text, &events);
  FakeServices services;
  dispatcher.set_services(&services);

  // Tail on: tail-native defaults now apply; the snapshot carries the zone.
  services.tail_on = true;
  SemanticCommand cmd;
  cmd.capability = SemanticCommand::Capability::AnimationSet;
  std::snprintf(cmd.name, sizeof(cmd.name), "comet");
  CommandResponse r = dispatcher.execute(cmd, 2000);
  CHECK(r.ok);
  ModeSnapshot snap = machine.snapshot(2000);
  CHECK_STR_EQ(snap.base_name, "comet");
  CHECK(snap.base_zone_target == ZoneTarget::Tail);
  CHECK(snap.current_zone_target == ZoneTarget::Tail);

  // "both" on a both-capable name.
  std::snprintf(cmd.name, sizeof(cmd.name), "heartbeat");
  cmd.has_target = true;
  std::snprintf(cmd.target, sizeof(cmd.target), "both");
  r = dispatcher.execute(cmd, 2001);
  CHECK(r.ok);
  CHECK(machine.snapshot(2001).base_zone_target == ZoneTarget::Both);

  // "both" on a tail-only name (no both capability) rejects.
  std::snprintf(cmd.name, sizeof(cmd.name), "comet");
  r = dispatcher.execute(cmd, 2002);
  CHECK(!r.ok);
  CHECK(r.error == ErrorCode::InvalidParameter);

  // Transient animation carries the target too.
  cmd.has_duration = true;
  cmd.duration_ms = 5000;
  std::snprintf(cmd.name, sizeof(cmd.name), "pulse");
  std::snprintf(cmd.target, sizeof(cmd.target), "both");
  r = dispatcher.execute(cmd, 2003);
  CHECK(r.ok);
  snap = machine.snapshot(2003);
  CHECK(snap.transient_active);
  CHECK(snap.transient_zone_target == ZoneTarget::Both);
  CHECK(snap.current_zone_target == ZoneTarget::Both);

  // Explicit matrix request on rainbow_flow (default matrix, tail capable).
  cmd.has_duration = false;
  std::snprintf(cmd.name, sizeof(cmd.name), "rainbow_flow");
  std::snprintf(cmd.target, sizeof(cmd.target), "matrix");
  r = dispatcher.execute(cmd, 2004);
  CHECK(r.ok);
  CHECK(machine.snapshot(2004).base_zone_target == ZoneTarget::Matrix);
  // …and its tail form applies as well.
  std::snprintf(cmd.target, sizeof(cmd.target), "tail");
  r = dispatcher.execute(cmd, 2005);
  CHECK(r.ok);
  CHECK(machine.snapshot(2005).base_zone_target == ZoneTarget::Tail);
}

void test_l010_config_tail_keys() {
  AppConfig config;
  // Defaults: tail disabled, placeholder length, no reverse.
  CHECK(!config.tail.enabled);
  CHECK_EQ_INT(config.tail.length, LED_WALL_TAIL_LENGTH_DEFAULT);
  CHECK(!config.tail.reverse);
  CHECK(config.ds4_enabled);  // DS4 build default-on (ignored elsewhere)

  ConfigEntryResult r = apply_config_entry(config, "tail.enabled", "1");
  CHECK(r.applied);
  CHECK(config.tail.enabled);
  r = apply_config_entry(config, "tail.length", "144");
  CHECK(r.applied);
  CHECK_EQ_INT(config.tail.length, 144);
  r = apply_config_entry(config, "tail.reverse", "1");
  CHECK(r.applied);
  CHECK(config.tail.reverse);
  r = apply_config_entry(config, "ds4.enabled", "0");
  CHECK(r.applied);
  CHECK(!config.ds4_enabled);

  // Invalid values fall back without mutation.
  r = apply_config_entry(config, "tail.length", "0");
  CHECK(!r.applied);
  CHECK_EQ_INT(config.tail.length, 144);
  r = apply_config_entry(config, "tail.length", "301");  // above the cap
  CHECK(!r.applied);
  r = apply_config_entry(config, "tail.enabled", "yes");
  CHECK(!r.applied);
  CHECK(config.tail.enabled);  // unchanged
  // Unknown keys still warn (legacy behavior intact).
  r = apply_config_entry(config, "tail.unknown", "1");
  CHECK(!r.applied);
}

void test_l010_public_state_zone_views() {
  ModeMachine machine;
  Brightness brightness;
  TextState text;
  Warnings warnings;
  machine.apply_persistent(ModeKind::Animation, "comet", 1000, ZoneTarget::Tail);
  DeviceStateViews views;
  views.tail_enabled = true;
  views.tail_length = 144;
  views.tail_reverse = true;
  views.tail_scan_index = 73;
  const char* target = "tail";
  views.animation_target = target;
  PublicState s = build_public_state(machine, brightness, text, warnings, 1000, 1, views);
  CHECK(s.tail_enabled);
  CHECK_EQ_INT(s.tail_length, 144);
  CHECK(s.tail_reverse);
  CHECK_EQ_INT(s.tail_scan_index, 73);
  CHECK_STR_EQ(s.animation_target, "tail");
  CHECK_STR_EQ(s.game_difficulty, "easy");
  CHECK_EQ_INT(s.game_players, 1);
  // Defaults without views: matrix target, placeholder length.
  PublicState d = build_public_state(machine, brightness, text, warnings, 1000, 1);
  CHECK(!d.tail_enabled);
  CHECK_EQ_INT(d.tail_length, LED_WALL_TAIL_LENGTH_DEFAULT);
  CHECK_EQ_INT(d.tail_scan_index, -1);
  CHECK_STR_EQ(d.animation_target, "matrix");
}
