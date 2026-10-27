#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>

#include "api/command.h"
#include "core/brightness.h"
#include "core/mode_machine.h"
#include "core/warnings.h"
#include "render/matrix_mapping.h"
#include "timer/countdown.h"

namespace ledwall {

// Public device state (frozen spec §4). Fields whose capabilities arrive with
// later phases (spectrum, clients) exist per the frozen state model and carry
// their neutral defaults until those phases implement them.
// Timebase contract (R-B02): `now_ms` is the same monotonic device clock the
// mode machine runs on; `uptime_seconds` is an independently derived §4
// display field (uptime since boot) and MUST NOT be fed back into machine
// timing.
struct PublicState {
  bool available{true};
  char device_id[32]{"led-wall"};  // "-<suffix>" added at runtime (see runtime.cpp)
  char device_type[24]{"visual_display_v1"};
  uint32_t protocol_version{1};
  char fw_version[24]{"0.1.0-alpha.1"};

  ModeKind base_mode{ModeKind::Idle};
  char base_name[kMaxProgramNameLen]{};
  ModeKind current_mode{ModeKind::Idle};
  char current_name[kMaxProgramNameLen]{};

  bool transient_active{false};
  ModeKind transient_kind{ModeKind::Idle};
  char transient_name[kMaxProgramNameLen]{};
  uint32_t transient_remaining_ms{0};

  char text_current[kTextMaxLen]{};  // empty unless text applies (§4)
  bool text_scrolling{false};
  uint32_t text_speed_ms_per_px{0};
  // LED-L009 Text Studio view of the applying slot:
  TextMode text_mode{TextMode::Fixed};
  bool text_rtl{true};
  bool text_loop{true};
  uint32_t text_word_dwell_ms{kWordDwellDefaultMs};
  TextStyle text_style{TextStyle::Solid};
  uint8_t text_fg_color{kDefaultTextFg};
  uint8_t text_bg_color{kDefaultTextBg};

  uint32_t brightness_current{0};
  uint32_t brightness_max{0};

  bool spectrum_available{false};  // advertised only if mic hardware confirmed (§14)
  bool spectrum_active{false};

  // LED-L009 device subsystem truth (reconstruction brief): the device owns
  // all of it; clients only observe. These views are refreshed by the runtime
  // before serialization.
  CountdownTimer::State timer_state{CountdownTimer::State::Idle};
  uint32_t timer_duration_ms{0};
  uint32_t timer_remaining_ms{0};

  bool menu_active{false};

  bool game_active{false};
  char game_name[kMaxProgramNameLen]{};
  char game_phase[16]{};
  uint32_t game_score{0};
  uint32_t game_score_p2{0};
  uint8_t game_winner{0};
  uint32_t game_high_score{0};
  bool game_high_score_available{false};
  char game_difficulty[8]{"easy"};  // V-L009-3: active engine difficulty
  uint8_t game_players{1};          // LED-L010: engine player mode (1|2)

  bool standby_on{false};

  bool notification_active{false};
  bool notification_high{false};
  char notification_text[kTextMaxLen]{};

  bool visualizer_active{false};
  bool sound_muted{false};

  // LED-L010 zone truth: the tail zone view + the current program's commanded
  // zone (reported when the current mode is scene/animation).
  bool tail_enabled{false};
  uint16_t tail_length{LED_WALL_TAIL_LENGTH_DEFAULT};  // config (Owner-counted)
  bool tail_reverse{false};
  int16_t tail_scan_index{-1};  // zero-based; -1 unless Tail scanner is active
  int16_t tail_setup_index{-1};  // zero-based steady diagnostic point; -1=off
  char animation_target[8]{"matrix"};  // matrix|tail|both (scene/animation only)

  // LED-L010 DS4 controller view (supported/enabled only in the esp32_eq_ds4
  // build; the canonical env reports supported:false).
  bool ds4_supported{false};
  bool ds4_enabled{false};
  char ds4_state[16]{"unsupported"};  // unsupported|unpaired|pairing|connected|disconnected|reconnecting
  uint8_t ds4_players{0};             // connected controllers
  uint8_t ds4_battery[2]{0, 0};       // per-slot 0..100 (0 = unknown)
#ifdef LED_WALL_ENABLE_DS4
  bool ds4_ready{false};
  uint8_t ds4_assigned_mask{0};
  uint32_t ds4_input_events{0};
#endif

  uint8_t matrix_width{kMatrixWidth};
  uint8_t matrix_height{kMatrixHeight};

  uint32_t clients_count{0};
  char warnings[kMaxWarnings][kMaxConfigWarningLen]{};
  uint8_t warning_count{0};

  uint32_t uptime_seconds{0};
};

// LED-L009 device subsystem views passed into the pure state builder
// (all defaulted — the builder stays usable for the pure §4 core).
struct DeviceStateViews {
  CountdownTimer::State timer_state{CountdownTimer::State::Idle};
  uint32_t timer_duration_ms{0};
  uint32_t timer_remaining_ms{0};
  bool menu_active{false};
  bool game_active{false};
  const char* game_name{""};
  const char* game_phase{""};
  uint32_t game_score{0};
  uint32_t game_score_p2{0};
  uint8_t game_winner{0};
  uint32_t game_high_score{0};
  bool game_high_score_available{false};
  const char* game_difficulty{"easy"};  // V-L009-3
  uint8_t game_players{1};              // LED-L010
  bool standby_on{false};
  bool notification_active{false};
  bool notification_high{false};
  const char* notification_text{""};
  bool visualizer_active{false};
  bool sound_muted{false};
  // LED-L010 views:
  bool tail_enabled{false};
  uint16_t tail_length{LED_WALL_TAIL_LENGTH_DEFAULT};
  bool tail_reverse{false};
  int16_t tail_scan_index{-1};
  int16_t tail_setup_index{-1};
  const char* animation_target{"matrix"};
  bool ds4_supported{false};
  bool ds4_enabled{false};
  const char* ds4_state{"unsupported"};
  uint8_t ds4_players{0};
  uint8_t ds4_battery_p1{0};
  uint8_t ds4_battery_p2{0};
#ifdef LED_WALL_ENABLE_DS4
  bool ds4_ready{false};
  uint8_t ds4_assigned_mask{0};
  uint32_t ds4_input_events{0};
#endif
};

inline PublicState build_public_state(const ModeMachine& machine, const Brightness& brightness,
                                      const TextState& text, const Warnings& warnings,
                                      uint32_t now_ms, uint32_t uptime_seconds,
                                      const DeviceStateViews& views = DeviceStateViews{}) {
  PublicState s;
  const ModeSnapshot snap = machine.snapshot(now_ms);
  s.base_mode = snap.base_mode;
  std::snprintf(s.base_name, kMaxProgramNameLen, "%s", snap.base_name);
  s.current_mode = snap.current_mode;
  std::snprintf(s.current_name, kMaxProgramNameLen, "%s", snap.current_name);
  s.transient_active = snap.transient_active;
  s.transient_kind = snap.transient_kind;
  std::snprintf(s.transient_name, kMaxProgramNameLen, "%s", snap.transient_name);
  s.transient_remaining_ms = snap.transient_remaining_ms;

  // §4: text {current, scrolling} "when text applies" — reported from the
  // program slot matching the current mode (transient text overrides base
  // text; expiry falls back to the base slot without any mutation).
  const TextProgram* text_slot = nullptr;
  if (snap.transient_active && snap.transient_kind == ModeKind::Text) {
    text_slot = &text.transient;
  } else if (snap.base_mode == ModeKind::Text) {
    text_slot = &text.base;
  }
  if (text_slot != nullptr) {
    std::snprintf(s.text_current, kTextMaxLen, "%s", text_slot->current);
    s.text_scrolling = text_slot->scrolling;
    s.text_speed_ms_per_px = text_slot->speed_ms_per_px;
    s.text_mode = text_slot->mode;
    s.text_rtl = text_slot->rtl;
    s.text_loop = text_slot->loop;
    s.text_word_dwell_ms = text_slot->word_dwell_ms;
    s.text_style = text_slot->style;
    s.text_fg_color = text_slot->fg_color;
    s.text_bg_color = text_slot->bg_color;
  }

  s.brightness_current = brightness.level();
  s.brightness_max = brightness.ceiling();

  for (uint8_t i = 0; i < warnings.count && i < kMaxWarnings; ++i) {
    std::snprintf(s.warnings[i], kMaxConfigWarningLen, "%s", warnings.items[i]);
  }
  s.warning_count = warnings.count;

  // LED-L009 device subsystem views.
  s.timer_state = views.timer_state;
  s.timer_duration_ms = views.timer_duration_ms;
  s.timer_remaining_ms = views.timer_remaining_ms;
  s.menu_active = views.menu_active;
  s.game_active = views.game_active;
  std::snprintf(s.game_name, kMaxProgramNameLen, "%s", views.game_name);
  std::snprintf(s.game_phase, sizeof(s.game_phase), "%s", views.game_phase);
  s.game_score = views.game_score;
  s.game_score_p2 = views.game_score_p2;
  s.game_winner = views.game_winner;
  s.game_high_score = views.game_high_score;
  s.game_high_score_available = views.game_high_score_available;
  std::snprintf(s.game_difficulty, sizeof(s.game_difficulty), "%s", views.game_difficulty);
  s.standby_on = views.standby_on;
  s.notification_active = views.notification_active;
  s.notification_high = views.notification_high;
  std::snprintf(s.notification_text, kTextMaxLen, "%s", views.notification_text);
  s.visualizer_active = views.visualizer_active;
  s.sound_muted = views.sound_muted;

  // LED-L010 views.
  s.game_players = views.game_players;
  s.tail_enabled = views.tail_enabled;
  s.tail_length = views.tail_length;
  s.tail_reverse = views.tail_reverse;
  s.tail_scan_index = views.tail_scan_index;
  s.tail_setup_index = views.tail_setup_index;
  std::snprintf(s.animation_target, sizeof(s.animation_target), "%s", views.animation_target);
  s.ds4_supported = views.ds4_supported;
  s.ds4_enabled = views.ds4_enabled;
  std::snprintf(s.ds4_state, sizeof(s.ds4_state), "%s", views.ds4_state);
  s.ds4_players = views.ds4_players;
  s.ds4_battery[0] = views.ds4_battery_p1;
  s.ds4_battery[1] = views.ds4_battery_p2;
#ifdef LED_WALL_ENABLE_DS4
  s.ds4_ready = views.ds4_ready;
  s.ds4_assigned_mask = views.ds4_assigned_mask;
  s.ds4_input_events = views.ds4_input_events;
#endif

  s.uptime_seconds = uptime_seconds;
  return s;
}

}  // namespace ledwall
