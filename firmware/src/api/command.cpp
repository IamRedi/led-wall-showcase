#include "api/command.h"

#include <cstring>

#include "api/device_services.h"
#include "games/game_host.h"
#include "input/input_events.h"
#include "render/matrix_mapping.h"
#include "text/text_renderer.h"
#include "timer/countdown.h"

namespace ledwall {

const char* error_code_name(ErrorCode code) {
  switch (code) {
    case ErrorCode::None:
      return "ok";
    case ErrorCode::TextTooWide:
      return "textTooWide";
    case ErrorCode::UnknownScene:
      return "unknownScene";
    case ErrorCode::UnknownAnimation:
      return "unknownAnimation";
    case ErrorCode::UnknownExpression:
      return "unknownExpression";
    case ErrorCode::InvalidParameter:
      return "invalidParameter";
    case ErrorCode::UnsupportedCapability:
      return "unsupportedCapability";
    case ErrorCode::LedUnavailable:
      return "ledUnavailable";
    case ErrorCode::InternalError:
      return "internalError";
    case ErrorCode::UnknownGame:
      return "unknownGame";
  }
  return "internalError";
}

const char* text_mode_name(TextMode mode) {
  if (static_cast<uint8_t>(mode) < kTextModeCount) {
    return kTextModeNames[static_cast<uint8_t>(mode)];
  }
  return nullptr;
}

bool text_mode_from_name(const char* name, TextMode* out) {
  if (name == nullptr) {
    return false;
  }
  for (uint8_t i = 0; i < kTextModeCount; ++i) {
    if (std::strcmp(name, kTextModeNames[i]) == 0) {
      if (out != nullptr) {
        *out = static_cast<TextMode>(i);
      }
      return true;
    }
  }
  return false;
}

bool text_tail_mode_from_name(const char* name, TextTailMode* out) {
  if (name == nullptr) return false;
  for (uint8_t i = 0; i < 7; ++i) {
    if (std::strcmp(name, kTextTailModeNames[i]) == 0) {
      if (out != nullptr) *out = static_cast<TextTailMode>(i);
      return true;
    }
  }
  return false;
}

CommandResponse CommandDispatcher::ok_(bool state_changed) {
  CommandResponse r;
  r.ok = true;
  r.state_changed = state_changed;
  return r;
}

CommandResponse CommandDispatcher::error_(ErrorCode code, const char* message, const char* available) {
  CommandResponse r;
  r.ok = false;
  r.error = code;
  if (message != nullptr) {
    std::strncpy(r.message, message, kMaxErrorMessageLen - 1);
    r.message[kMaxErrorMessageLen - 1] = '\0';
  }
  if (available != nullptr) {
    std::strncpy(r.available, available, kMaxAvailableListLen - 1);
    r.available[kMaxAvailableListLen - 1] = '\0';
  }
  return r;
}

void CommandDispatcher::begin(ModeMachine* machine, Brightness* brightness, TextState* text,
                              EventSink* events) {
  machine_ = machine;
  brightness_ = brightness;
  text_ = text;
  events_ = events;
}

void CommandDispatcher::set_services(DeviceServices* services) { services_ = services; }

CommandResponse CommandDispatcher::apply_program_(ModeKind kind, const char* name,
                                                  const SemanticCommand& cmd, uint32_t now_ms) {
  return apply_program_(kind, name, cmd, now_ms, ZoneTarget::Matrix);
}

// LED-L010: target-aware program application (additive — the legacy form above
// keeps the accepted Matrix behavior).
CommandResponse CommandDispatcher::apply_program_(ModeKind kind, const char* name,
                                                  const SemanticCommand& cmd, uint32_t now_ms,
                                                  ZoneTarget target) {
  const ModeSnapshot before = machine_->snapshot(now_ms);
  if (cmd.has_duration) {
    if (cmd.duration_ms == 0 || cmd.duration_ms > kMaxTransientDurationMs) {
      return error_(ErrorCode::InvalidParameter,
                    "durationMs must be a positive integer within the supported range", nullptr);
    }
    machine_->apply_transient(kind, name, cmd.duration_ms, now_ms, target);
  } else {
    machine_->apply_persistent(kind, name, now_ms, target);  // cancels any transient (§11)
  }
  (void)target;
  const ModeSnapshot after = machine_->snapshot(now_ms);
  // §9: every accepted program command mutates device state (machine dirty),
  // so state.changed is always emitted; mode.changed only when the kind flips.
  events_->push(EventKind::StateChanged);
  if (before.current_mode != after.current_mode) {
    events_->push(EventKind::ModeChanged);
  }
  return ok_(true);
}

// LED-L010: resolve + validate the commanded zone target for scene/animation
// programs. Absent target = the animation's default (registry metadata).
CommandResponse CommandDispatcher::resolve_zone_target_(const SemanticCommand& cmd,
                                                        ZoneTarget* out) {
  ZoneTarget resolved = ZoneTarget::Matrix;
  if (cmd.has_target) {
    if (!zone_target_from_name(cmd.target, &resolved)) {
      return error_(ErrorCode::InvalidParameter,
                    "target must be matrix, tail or both", nullptr);
    }
  } else {
    resolved = animation_default_target(cmd.name);
  }
  // Tail-involving targets need the tail zone (driver/config truth).
  if (resolved != ZoneTarget::Matrix) {
    if (services_ == nullptr || !services_->tail_enabled()) {
      return error_(ErrorCode::LedUnavailable,
                    "the tail zone is disabled; enable it in the device config", nullptr);
    }
  }
  const uint8_t mask = animation_zone_mask(cmd.name);
  if (!zone_target_supported(mask, resolved)) {
    return error_(ErrorCode::InvalidParameter,
                  "this animation does not support the requested target", nullptr);
  }
  *out = resolved;
  CommandResponse ok;
  ok.ok = true;
  return ok;
}

CommandResponse CommandDispatcher::execute(const SemanticCommand& cmd, uint32_t now_ms) {
  const CommandResponse result = execute_inner_(cmd, now_ms);
  if (result.ok && services_ != nullptr && cmd.capability != SemanticCommand::Capability::TailPoint &&
      cmd.capability != SemanticCommand::Capability::StateRead &&
      cmd.capability != SemanticCommand::Capability::BrightnessSet) {
    services_->tail_point_off();
  }
  return result;
}

CommandResponse CommandDispatcher::execute_inner_(const SemanticCommand& cmd, uint32_t now_ms) {
  if (machine_ == nullptr || brightness_ == nullptr || text_ == nullptr || events_ == nullptr) {
    return error_(ErrorCode::InternalError, "dispatcher not initialized", nullptr);
  }

  switch (cmd.capability) {
    case SemanticCommand::Capability::TailPoint: {
      // Diagnostic bound only; it is not a measured physical count.
      if (!cmd.has_tail_index || cmd.tail_index >= 200) {
        return error_(ErrorCode::InvalidParameter, "index must be 0..199", nullptr);
      }
      if (services_ == nullptr || !services_->tail_enabled()) {
        return error_(ErrorCode::LedUnavailable, "the Tail is disabled", nullptr);
      }
      if (cmd.tail_index >= services_->tail_length() || !services_->tail_point_set(cmd.tail_index)) {
        return error_(ErrorCode::InvalidParameter, "index exceeds the configured Tail scan bound", nullptr);
      }
      machine_->clear(now_ms);
      brightness_->set_level(5);
      events_->push(EventKind::StateChanged);
      events_->push(EventKind::ModeChanged);
      return ok_(true);
    }
    case SemanticCommand::Capability::TailPointOff: {
      if (services_ != nullptr) services_->tail_point_off();
      machine_->clear(now_ms);
      events_->push(EventKind::StateChanged);
      events_->push(EventKind::ModeChanged);
      return ok_(true);
    }
    case SemanticCommand::Capability::TextShow: {
      const size_t text_len = std::strlen(cmd.text);
      // By construction the SemanticCommand struct cannot hold more than
      // kTextProtocolCap characters (transports reject longer input before
      // building the command), so only the empty check applies here.
      if (text_len == 0 || text_len > kTextProtocolCap) {
        return error_(ErrorCode::InvalidParameter, "text must not be empty", nullptr);
      }
      if (cmd.has_speed && (cmd.speed_ms_per_px < kScrollSpeedMinMsPerPx ||
                            cmd.speed_ms_per_px > kScrollSpeedMaxMsPerPx)) {
        return error_(ErrorCode::InvalidParameter,
                      "speedMsPerPx must be within 40..500", nullptr);
      }
      if (cmd.has_word_dwell && (cmd.word_dwell_ms < kWordDwellMinMs ||
                                 cmd.word_dwell_ms > kWordDwellMaxMs)) {
        return error_(ErrorCode::InvalidParameter,
                      "wordDwellMs must be within 200..60000", nullptr);
      }
      // LED-L009 Text Studio mode resolution:
      // - explicit mode wins;
      // - legacy scroll flag maps exactly as before (scroll:false → static,
      //   scroll:true → heritage LTR marquee);
      // - §6 overflow rule (render-time determination): only Fixed can be
      //   too wide — marquee and words render any length.
      TextMode mode = TextMode::Fixed;
      bool rtl = true;  // product default reading direction for explicit marquee
      if (cmd.has_mode) {
        mode = cmd.mode;
      } else {
        const bool scrolling = cmd.has_scroll ? cmd.scroll : false;
        if (scrolling) {
          mode = TextMode::Marquee;
          rtl = false;  // legacy scroll keeps the heritage left→right direction
        }
      }
      if (cmd.has_direction) {
        rtl = cmd.rtl;
      }
      if (mode == TextMode::Fixed && !text::fits_static(cmd.text)) {
        return error_(ErrorCode::TextTooWide,
                      "text is wider than the canvas; retry with scroll:true", nullptr);
      }

      TextProgram program;
      std::strncpy(program.current, cmd.text, kTextMaxLen - 1);
      program.current[kTextMaxLen - 1] = '\0';
      program.scrolling = (mode == TextMode::Marquee);  // legacy projection
      program.scroll_explicit = cmd.has_scroll;
      program.speed_ms_per_px = cmd.has_speed ? cmd.speed_ms_per_px : kScrollSpeedDefaultMsPerPx;
      program.started_ms = now_ms;   // program-relative scroll/render timing
      program.done_reported = false;  // text.done once per program activation
      program.mode = mode;
      program.rtl = rtl;
      program.loop = cmd.has_loop ? cmd.loop : true;
      program.word_dwell_ms = cmd.has_word_dwell ? cmd.word_dwell_ms : kWordDwellDefaultMs;
      program.style = cmd.has_style ? cmd.style : TextStyle::Solid;
      program.fg_color = cmd.has_fg_color ? cmd.fg_color : kDefaultTextFg;
      program.bg_color = cmd.has_bg_color ? cmd.bg_color : kDefaultTextBg;
      program.tail_mode = cmd.has_tail_mode ? cmd.tail_mode : TextTailMode::Off;
      program.tail_color = cmd.has_tail_color ? cmd.tail_color : kDefaultTextFg;
      program.duration_ms = cmd.has_duration ? cmd.duration_ms : 0;

      const ModeSnapshot before = machine_->snapshot(now_ms);
      if (cmd.has_duration) {
        if (cmd.duration_ms == 0 || cmd.duration_ms > kMaxTransientDurationMs) {
          return error_(ErrorCode::InvalidParameter,
                        "durationMs must be a positive integer within the supported range", nullptr);
        }
        machine_->apply_transient(ModeKind::Text, "", cmd.duration_ms, now_ms);
        text_->transient = program;
      } else {
        machine_->apply_persistent(ModeKind::Text, "", now_ms);
        text_->base = program;
        text_->transient = TextProgram{};  // persistent command cancels transient content
      }
      const ModeSnapshot after = machine_->snapshot(now_ms);
      events_->push(EventKind::StateChanged);
      if (before.current_mode != after.current_mode) {
        events_->push(EventKind::ModeChanged);
      }
      CommandResponse response = ok_(true);
      response.has_skipped_characters = true;  // §6: ack reports skippedCharacters
      response.skipped_characters =
          static_cast<uint8_t>(text::count_skipped_characters(cmd.text));
      return response;
    }

    case SemanticCommand::Capability::TextClear: {
      // §11 CLEAR-2: scoped to text only; never restores a historical stack.
      const ModeSnapshot snap = machine_->snapshot(now_ms);
      if (snap.base_mode == ModeKind::Text) {
        machine_->replace_base(ModeKind::Idle, "", now_ms);
        text_->base = TextProgram{};
        events_->push(EventKind::StateChanged);
        events_->push(EventKind::ModeChanged);
        return ok_(true);
      }
      if (snap.transient_active && snap.transient_kind == ModeKind::Text) {
        machine_->cancel_transient(now_ms);
        text_->transient = TextProgram{};
        events_->push(EventKind::StateChanged);
        events_->push(EventKind::ModeChanged);
        return ok_(true);
      }
      return ok_(false);  // no text program active: no-op, no event
    }

    case SemanticCommand::Capability::SceneSet: {
      if (!valid_scene(cmd.name)) {
        char available[kMaxAvailableListLen];
        list_names(kAnimationNames, kAnimationCount, available, sizeof(available));
        return error_(ErrorCode::UnknownScene, "unknown scene", available);
      }
      ZoneTarget target = ZoneTarget::Matrix;
      const CommandResponse zone = resolve_zone_target_(cmd, &target);
      if (!zone.ok) {
        return zone;
      }
      return apply_program_(ModeKind::Scene, cmd.name, cmd, now_ms, target);
    }

    case SemanticCommand::Capability::AnimationSet: {
      if (!valid_animation(cmd.name)) {
        char available[kMaxAvailableListLen];
        list_names(kAnimationNames, kAnimationCount, available, sizeof(available));
        return error_(ErrorCode::UnknownAnimation, "unknown animation", available);
      }
      ZoneTarget target = ZoneTarget::Matrix;
      const CommandResponse zone = resolve_zone_target_(cmd, &target);
      if (!zone.ok) {
        return zone;
      }
      return apply_program_(ModeKind::Animation, cmd.name, cmd, now_ms, target);
    }

    case SemanticCommand::Capability::ExpressionSet: {
      if (!valid_expression(cmd.name)) {
        char available[kMaxAvailableListLen];
        list_names(kExpressionNames, kExpressionCount, available, sizeof(available));
        return error_(ErrorCode::UnknownExpression, "unknown expression", available);
      }
      if (cmd.has_duration) {
        return error_(ErrorCode::InvalidParameter,
                      "expression.set takes no durationMs (persistent intent per §5/§8)", nullptr);
      }
      // Repair cycle 1 (review minor, strict-parse heritage §17): target is
      // not part of expression/set — expressions map data-driven over both
      // zones. Rejected instead of silently ignored.
      if (cmd.has_target) {
        return error_(ErrorCode::InvalidParameter,
                      "expression.set takes no target (expressions realize their semantic mapping on both zones)", nullptr);
      }
      return apply_program_(ModeKind::Expression, cmd.name, cmd, now_ms);
    }

    case SemanticCommand::Capability::Clear: {
      // §11 CLEAR-1: cancel transient, baseMode = idle, no automatic animation.
      // V5: Clear is also a lifecycle exit. A visual-only clear left the game
      // engine running in the background and allowed its score to advance.
      if (services_ != nullptr) {
        services_->game_exit();
        services_->visualizer_clear();
        services_->tail_point_off();
      }
      machine_->clear(now_ms);
      text_->base = TextProgram{};
      text_->transient = TextProgram{};
      events_->push(EventKind::StateChanged);
      events_->push(EventKind::ModeChanged);
      return ok_(true);
    }

    case SemanticCommand::Capability::BrightnessSet: {
      if (!cmd.has_level) {
        return error_(ErrorCode::InvalidParameter, "level is required (0..100)", nullptr);
      }
      if (services_ != nullptr && services_->tail_point_active() && cmd.level > 5) {
        return error_(ErrorCode::InvalidParameter, "Tail Setup brightness is limited to 5", nullptr);
      }
      // §5/§13: requests clamp to the locally enforced ceiling — never rejected.
      // Standby rule: brightness adjustments never wake the display (§21).
      const bool changed = brightness_->set_level(cmd.level);
      if (changed) {
        events_->push(EventKind::StateChanged);
      }
      return ok_(changed);
    }

    // ------------------------------------------------ LED-L009 additions ----
    case SemanticCommand::Capability::TimerSet: {
      if (services_ == nullptr) {
        return error_(ErrorCode::InternalError, "device services unavailable", nullptr);
      }
      if (!cmd.has_duration || cmd.duration_ms == 0 ||
          cmd.duration_ms > CountdownTimer::kMaxDurationMs) {
        return error_(ErrorCode::InvalidParameter,
                      "durationMs must be a positive integer within the supported range", nullptr);
      }
      services_->timer_set(cmd.duration_ms);
      events_->push(EventKind::StateChanged);
      return ok_(true);
    }
    case SemanticCommand::Capability::TimerStart: {
      if (services_ == nullptr) {
        return error_(ErrorCode::InternalError, "device services unavailable", nullptr);
      }
      if (!services_->timer_start(now_ms)) {
        return error_(ErrorCode::InvalidParameter, "no countdown duration is set", nullptr);
      }
      // Starting the countdown shows it: base becomes timer (wakes standby).
      const ModeSnapshot before = machine_->snapshot(now_ms);
      machine_->apply_persistent(ModeKind::Timer, "", now_ms);
      events_->push(EventKind::StateChanged);
      if (before.current_mode != ModeKind::Timer) {
        events_->push(EventKind::ModeChanged);
      }
      return ok_(true);
    }
    case SemanticCommand::Capability::TimerPause: {
      if (services_ == nullptr) {
        return error_(ErrorCode::InternalError, "device services unavailable", nullptr);
      }
      if (!services_->timer_pause(now_ms)) {
        return ok_(false);  // not running: no-op
      }
      events_->push(EventKind::StateChanged);
      return ok_(true);
    }
    case SemanticCommand::Capability::TimerResume: {
      if (services_ == nullptr) {
        return error_(ErrorCode::InternalError, "device services unavailable", nullptr);
      }
      if (!services_->timer_resume(now_ms)) {
        return ok_(false);  // not paused: no-op
      }
      events_->push(EventKind::StateChanged);
      return ok_(true);
    }
    case SemanticCommand::Capability::TimerReset: {
      if (services_ == nullptr) {
        return error_(ErrorCode::InternalError, "device services unavailable", nullptr);
      }
      // Reset re-arms the stored duration and stays in the timer display.
      services_->timer_reset(now_ms);
      if (machine_->snapshot(now_ms).base_mode == ModeKind::Standby) {
        return ok_(true);  // standby stays untouched (engine reset only)
      }
      events_->push(EventKind::StateChanged);
      return ok_(true);
    }
    case SemanticCommand::Capability::TimerStop: {
      if (services_ == nullptr) {
        return error_(ErrorCode::InternalError, "device services unavailable", nullptr);
      }
      // STOP is semantically distinct from reset: re-arm AND leave the timer
      // display (back to idle) — the wall stops showing the countdown.
      services_->timer_reset(now_ms);
      const ModeSnapshot before = machine_->snapshot(now_ms);
      if (before.base_mode == ModeKind::Timer) {
        machine_->replace_base(ModeKind::Idle, "", now_ms);
        events_->push(EventKind::ModeChanged);
      }
      events_->push(EventKind::StateChanged);
      return ok_(true);
    }
    case SemanticCommand::Capability::InputSend: {
      if (services_ == nullptr) {
        return error_(ErrorCode::InternalError, "device services unavailable", nullptr);
      }
      InputEvent ev = InputEvent::Up;
      if (!cmd.has_input || !input_event_from_name(cmd.input, &ev)) {
        return error_(ErrorCode::InvalidParameter,
                      "input must be one of the normalized event names", nullptr);
      }
      // LED-L010: optional player attribution (1|2; absent = P1).
      uint8_t player = kPlayer1;
      if (cmd.has_player) {
        if (cmd.player < kPlayer1 || cmd.player > kPlayerMax) {
          return error_(ErrorCode::InvalidParameter, "player must be 1 or 2", nullptr);
        }
        player = cmd.player;
      }
      if (!services_->input_send_player(player, ev)) {
        return ok_(false);  // bounded queue rejected (overflow): no-op
      }
      return ok_(true);
    }
    case SemanticCommand::Capability::MenuSet: {
      if (services_ == nullptr) {
        return error_(ErrorCode::InternalError, "device services unavailable", nullptr);
      }
      if (!cmd.has_action) {
        return error_(ErrorCode::InvalidParameter, "action (enter|exit) is required", nullptr);
      }
      bool ok = false;
      if (std::strcmp(cmd.action, "enter") == 0) {
        ok = services_->menu_enter(now_ms);
        if (ok) {
          machine_->apply_persistent(ModeKind::Menu, "", now_ms);
        }
      } else if (std::strcmp(cmd.action, "exit") == 0) {
        ok = services_->menu_exit();
        if (ok && machine_->snapshot(now_ms).base_mode == ModeKind::Menu) {
          machine_->replace_base(ModeKind::Idle, "", now_ms);
        }
      } else {
        return error_(ErrorCode::InvalidParameter, "action must be enter or exit", nullptr);
      }
      if (ok) {
        events_->push(EventKind::StateChanged);
        events_->push(EventKind::ModeChanged);
      }
      return ok_(ok);
    }
    case SemanticCommand::Capability::GameSet: {
      if (services_ == nullptr) {
        return error_(ErrorCode::InternalError, "device services unavailable", nullptr);
      }
      if (!valid_game(cmd.name)) {
        char available[kMaxAvailableListLen];
        list_names(kGameNames, kGameCount, available, sizeof(available));
        return error_(ErrorCode::UnknownGame, "unknown game", available);
      }
      // LED-L010: optional player mode — validated against the game's registry
      // metadata (solo-only games reject players:2); applies at reset.
      if (cmd.has_players) {
        if (cmd.players < 1 || cmd.players > 2) {
          return error_(ErrorCode::InvalidParameter, "players must be 1 or 2", nullptr);
        }
        if (cmd.players == 2 && game_max_players(cmd.name) < 2) {
          return error_(ErrorCode::InvalidParameter,
                        "this game does not support 2 players", nullptr);
        }
      }
      if (game_min_players(cmd.name) == 2 && (!cmd.has_players || cmd.players != 2)) {
        return error_(ErrorCode::InvalidParameter,
                      "this game requires 2 players", nullptr);
      }
      // Repair cycle 1 (M-L010-2/3): explicit supported-lane rule — tail lane
      // games are rejected when the CONFIGURED tail lane is below the game's
      // registry minimum instead of silently running on a substituted lane.
      if (game_min_lane(cmd.name) > services_->tail_length()) {
        return error_(ErrorCode::InvalidParameter,
                      "tail lane too short for this game", nullptr);
      }
      if (cmd.has_players) services_->game_set_players(static_cast<int>(cmd.players));
      if (!services_->game_start(cmd.name, now_ms)) {
        return error_(ErrorCode::InternalError, "game could not start", nullptr);
      }
      const ModeSnapshot before = machine_->snapshot(now_ms);
      machine_->apply_persistent(ModeKind::Game, cmd.name, now_ms);
      events_->push(EventKind::StateChanged);
      if (before.current_mode != ModeKind::Game) {
        events_->push(EventKind::ModeChanged);
      }
      return ok_(true);
    }
    case SemanticCommand::Capability::GameCommand: {
      if (services_ == nullptr) {
        return error_(ErrorCode::InternalError, "device services unavailable", nullptr);
      }
      if (!cmd.has_action) {
        return error_(ErrorCode::InvalidParameter,
                      "action (pause|resume|restart|exit) is required", nullptr);
      }
      if (std::strcmp(cmd.action, "exit") == 0) {
        services_->game_exit();
        if (machine_->snapshot(now_ms).base_mode == ModeKind::Game) {
          machine_->replace_base(ModeKind::Idle, "", now_ms);
        }
        events_->push(EventKind::StateChanged);
        events_->push(EventKind::ModeChanged);
        return ok_(true);
      }
      if (std::strcmp(cmd.action, "pause") != 0 && std::strcmp(cmd.action, "resume") != 0 &&
          std::strcmp(cmd.action, "restart") != 0) {
        return error_(ErrorCode::InvalidParameter,
                      "action must be pause, resume, restart or exit", nullptr);
      }
      const bool changed = services_->game_command(cmd.action, now_ms);
      if (changed) {
        events_->push(EventKind::StateChanged);
      }
      return ok_(changed);
    }
    case SemanticCommand::Capability::GameDifficulty: {
      // V-L009-3: the level is engine state — the phone selects, the device
      // enforces. Persists across game sessions; applies at the next
      // start/restart of the active game.
      if (services_ == nullptr) {
        return error_(ErrorCode::InternalError, "device services unavailable", nullptr);
      }
      games::GameDifficulty d = games::GameDifficulty::Easy;
      if (!cmd.has_difficulty ||
          !games::game_difficulty_from_name(cmd.difficulty, &d)) {
        return error_(ErrorCode::InvalidParameter,
                      "difficulty must be easy, normal or hard", nullptr);
      }
      services_->game_set_difficulty(static_cast<int>(d));
      events_->push(EventKind::StateChanged);
      return ok_(true);
    }
    case SemanticCommand::Capability::VisualizerStart: {
      const ModeSnapshot before = machine_->snapshot(now_ms);
      machine_->apply_persistent(ModeKind::Spectrum, "", now_ms);
      events_->push(EventKind::StateChanged);
      if (before.current_mode != ModeKind::Spectrum) {
        events_->push(EventKind::ModeChanged);
      }
      return ok_(true);
    }
    case SemanticCommand::Capability::VisualizerStop: {
      services_->visualizer_clear();
      if (machine_->snapshot(now_ms).base_mode == ModeKind::Spectrum) {
        machine_->replace_base(ModeKind::Idle, "", now_ms);
      }
      events_->push(EventKind::StateChanged);
      events_->push(EventKind::ModeChanged);
      return ok_(true);
    }
    case SemanticCommand::Capability::VisualizerData: {
      if (services_ == nullptr) {
        return error_(ErrorCode::InternalError, "device services unavailable", nullptr);
      }
      // Bounded semantic input: exactly kSpectrumBands values 0..255,
      // rate-limited inside the service; malformed feeds mutate nothing.
      if (!services_->visualizer_feed(cmd.bands, kSpectrumBands, now_ms)) {
        return error_(ErrorCode::InvalidParameter,
                      "visualizer feed rejected (rate limit or payload shape)", nullptr);
      }
      return ok_(true);  // feed data is not §4 state: no state.changed
    }
    case SemanticCommand::Capability::Notify: {
      if (services_ == nullptr) {
        return error_(ErrorCode::InternalError, "device services unavailable", nullptr);
      }
      const size_t text_len = std::strlen(cmd.text);
      if (text_len == 0 || text_len > kTextProtocolCap) {
        return error_(ErrorCode::InvalidParameter, "text must not be empty", nullptr);
      }
      uint32_t duration = 5000;  // documented default
      if (cmd.has_duration) {
        if (cmd.duration_ms == 0 || cmd.duration_ms > 300000u) {
          return error_(ErrorCode::InvalidParameter,
                        "durationMs must be within 1..300000 for notifications", nullptr);
        }
        duration = cmd.duration_ms;
      }
      bool high = false;
      if (cmd.has_priority) {
        if (std::strcmp(cmd.priority, "high") == 0) {
          high = true;
        } else if (std::strcmp(cmd.priority, "normal") != 0) {
          return error_(ErrorCode::InvalidParameter, "priority must be normal or high", nullptr);
        }
      }
      services_->notify_store(cmd.text, high);  // payload before activation
      machine_->apply_transient(ModeKind::Notification, "", duration, now_ms);
      services_->play_cue(3);  // SoundCue::Notification
      events_->push(EventKind::StateChanged);
      return ok_(true);
    }
    case SemanticCommand::Capability::StandbySet: {
      if (!cmd.has_on) {
        return error_(ErrorCode::InvalidParameter, "on (boolean) is required", nullptr);
      }
      const ModeSnapshot before = machine_->snapshot(now_ms);
      if (cmd.on) {
        machine_->apply_persistent(ModeKind::Standby, "", now_ms);
      } else if (before.base_mode == ModeKind::Standby) {
        machine_->replace_base(ModeKind::Idle, "", now_ms);  // wake
      }
      events_->push(EventKind::StateChanged);
      events_->push(EventKind::ModeChanged);
      return ok_(true);
    }

    case SemanticCommand::Capability::StateRead:
      return ok_(false);  // transport serializes the current snapshot
  }

  return error_(ErrorCode::InternalError, "unhandled capability", nullptr);
}

}  // namespace ledwall
