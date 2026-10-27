#include "core/runtime.h"

#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <cstdio>
#include <cstring>

#include "api/state_codec.h"
#include "effects/tail_renderers.h"
#include "input/input_pins.h"
#include "render/output_limiter.h"
#include "text/text_renderer.h"
#ifdef LED_WALL_ENABLE_DS4
#include "input/ds4_provider_bluepad32.h"
#endif

namespace ledwall {

namespace {

char device_id_buf[32];
Runtime* g_runtime = nullptr;  // single firmware instance; providers use it

const char* resolve_device_id() {
  // Stable `led-wall-<suffix>` identity (frozen spec §10): suffix = low 24 bits
  // of the ESP32 efuse MAC.
  if (device_id_buf[0] == '\0') {
    std::snprintf(device_id_buf, sizeof(device_id_buf), "led-wall-%06llx",
                  static_cast<unsigned long long>(ESP.getEfuseMac() & 0xFFFFFFu));
  }
  return device_id_buf;
}

#ifdef LED_WALL_ENABLE_DS4
constexpr const char* kFwVersion = "0.5.1-alpha.1";  // LED-L014 DS4 candidate
#else
constexpr const char* kFwVersion = "0.5.0-alpha.1";  // LED-L013 V5 candidate
#endif

Identity identity_provider() {
  return g_runtime != nullptr ? g_runtime->identity() : Identity{};
}

PublicState state_provider() {
  // Same monotonic clock as the tick loop (R-B02 timebase contract).
  return g_runtime != nullptr ? g_runtime->state(millis()) : PublicState{};
}

uint32_t pixel_session_provider() {
  return g_runtime != nullptr ? g_runtime->begin_pixel_session() : 0;
}

PixelCanvas::ApplyResult pixel_frame_provider(uint32_t session, uint32_t revision,
                                                const char* pixels, size_t length,
                                                bool live, uint32_t now_ms) {
  return g_runtime != nullptr
             ? g_runtime->apply_pixel_frame(session, revision, pixels, length, live, now_ms)
             : PixelCanvas::ApplyResult::StaleSession;
}

bool scene_play_provider(const SceneSpec& spec, uint32_t now_ms) {
  return g_runtime != nullptr && g_runtime->play_scene(spec, now_ms);
}

void scene_stop_provider(uint32_t now_ms) {
  if (g_runtime != nullptr) g_runtime->stop_scene(now_ms);
}

void sound_mute_provider(bool muted) {
  if (g_runtime != nullptr) g_runtime->set_sound_muted(muted);
}

// Timer display arm default when the menu opens the timer with no duration
// set yet (brief §13: the menu must be usable standalone).
constexpr uint32_t kMenuDefaultTimerDurationMs = 10000;

void game_score_key(const char* name, games::GameDifficulty difficulty,
                    uint8_t players, char out[16]) {
  uint32_t hash = 2166136261u;
  for (const unsigned char* p = reinterpret_cast<const unsigned char*>(name); *p; ++p) {
    hash = (hash ^ *p) * 16777619u;
  }
  std::snprintf(out, 16, "r1_%08lx_%u%u", static_cast<unsigned long>(hash),
                static_cast<unsigned>(difficulty), static_cast<unsigned>(players));
}

}  // namespace

// ------------------------------------------------------------- services ----
// DeviceServices bridge: the pure dispatcher decides, these methods execute
// against the owned subsystems. Sound cues use raw ints to keep the interface
// leaf-clean (values = SoundCue order).
class Runtime::RuntimeServices final : public DeviceServices {
 public:
  explicit RuntimeServices(Runtime* r) : runtime(r) {}
  Runtime* runtime{nullptr};

  bool timer_set(uint32_t duration_ms) override {
    runtime->timer_.arm(duration_ms);
    return true;
  }
  bool timer_start(uint32_t now_ms) override { return runtime->timer_.start(now_ms); }
  bool timer_pause(uint32_t now_ms) override { return runtime->timer_.pause(now_ms); }
  bool timer_resume(uint32_t now_ms) override { return runtime->timer_.resume(now_ms); }
  void timer_reset(uint32_t now_ms) override { runtime->timer_.reset(now_ms); }

  bool input_send(InputEvent event) override {
    runtime->input_queue_.push(event);
    return true;
  }
  // LED-L010: player-attributed API events (phone controller P1/P2).
  bool input_send_player(uint8_t player, InputEvent event) override {
    runtime->input_queue_.push(player, event);
    return true;
  }

  bool menu_enter(uint32_t now_ms) override {
    runtime->enter_menu_(now_ms);
    return true;
  }
  bool menu_exit() override {
    if (!runtime->menu_.active()) {
      return false;
    }
    runtime->exit_menu_(millis());
    return true;
  }

  bool game_start(const char* name, uint32_t now_ms) override {
    return runtime->game_host_.start_by_name(name, now_ms);
  }
  bool game_command(const char* action, uint32_t /*now_ms*/) override {
    games::Game* g = runtime->game_host_.current_game();
    if (g == nullptr) {
      return false;
    }
    const bool pausing = std::strcmp(action, "pause") == 0;
    const bool resuming = std::strcmp(action, "resume") == 0;
    const bool restarting = std::strcmp(action, "restart") == 0;
    if (pausing && g->phase() == games::GamePhase::Running) {
      g->set_phase(games::GamePhase::Paused);
      return true;
    }
    if (resuming && g->phase() == games::GamePhase::Paused) {
      g->set_phase(games::GamePhase::Running);
      return true;
    }
    if (restarting) {
      g->reset(millis());
      return true;
    }
    return false;
  }
  void game_exit() override { runtime->game_host_.exit(); }

  void game_set_difficulty(int difficulty) override {
    // V-L009-3: raw int at the seam (leaf-clean interface, play_cue pattern);
    // clamped to the valid enum range, fail closed to Easy.
    if (difficulty < 0 || difficulty > static_cast<int>(games::GameDifficulty::Hard)) {
      difficulty = 0;
    }
    runtime->game_host_.set_difficulty(static_cast<games::GameDifficulty>(difficulty));
  }

  // LED-L010: engine player mode (1|2; applies at the next start/restart).
  void game_set_players(int players) override {
    runtime->game_host_.set_players(players <= 1 ? 1u : 2u);
  }

  // LED-L010: tail zone availability (drives the ledUnavailable gate).
  bool tail_enabled() const override { return runtime->config_.tail.enabled; }

  // Repair cycle 1 (M-L010-2/3): configured lane length for the tail-game
  // minimum-lane validation.
  uint16_t tail_length() const override { return runtime->config_.tail.length; }

  bool tail_point_set(uint16_t index) override {
    if (!runtime->config_.tail.enabled || index >= runtime->config_.tail.length || index >= 200) return false;
    runtime->tail_point_index_ = static_cast<int16_t>(index);
    runtime->force_render_ = true;
    return true;
  }
  void tail_point_off() override {
    if (runtime->tail_point_index_ >= 0) {
      runtime->tail_point_index_ = -1;
      runtime->force_render_ = true;
    }
  }
  bool tail_point_active() const override { return runtime->tail_point_index_ >= 0; }

  bool visualizer_feed(const uint8_t* bands, uint8_t count, uint32_t now_ms) override {
    return runtime->spectrum_.feed(bands, count, now_ms);
  }
  void visualizer_clear() override { runtime->spectrum_.clear(); }

  void notify_store(const char* text, bool high_priority) override {
    std::snprintf(runtime->notify_.text, sizeof(runtime->notify_.text), "%s", text);
    runtime->notify_.high_priority = high_priority;
    runtime->notify_.active = true;  // maintained against the transient each tick
  }

  void play_cue(int cue) override {
    if (runtime->sound_ != nullptr && cue >= 0 && cue <= static_cast<int>(SoundCue::Button)) {
      runtime->sound_->play(static_cast<SoundCue>(cue));
    }
  }
};

// ------------------------------------------------------ menu actions ----
// Nested member class: needs access to the Runtime's private subsystems.
class Runtime::RuntimeMenuActions final : public MenuActions {
 public:
  Runtime* runtime{nullptr};
  uint32_t brightness_level() const override { return runtime->brightness_.level(); }
  bool standby_on() const override {
    return runtime->machine_.snapshot(millis()).base_mode == ModeKind::Standby;
  }
};

void Runtime::begin(uint32_t now_ms) {
  boot_ms_ = now_ms;
  g_runtime = this;
  load_config(&config_, &warnings_);

  sound_ = &sound_scheduler_;  // logical cues; physical sink remains disconnected
  if (services_ == nullptr) {
    services_ = new RuntimeServices(this);
  }
  (void)menu_actions_device();  // initialize the single menu-actions instance

  brightness_.configure(config_.brightness_default_level, config_.brightness_ceiling);
  machine_.boot(config_.boot_base_mode, now_ms);
  dispatcher_.begin(&machine_, &brightness_, &text_, &events_);
  dispatcher_.set_services(services_);
  game_host_.begin(sound_);
  game_host_.set_lane_length(config_.tail.length);  // LED-L010: tail lane bound
  gpio_inputs_.begin(config_.input_enabled, input_pins::kCandidateMap);
  led_driver_begin(config_.orientation);
  // LED-L010: tail zone (config default disabled — no second controller, no
  // tail pin signal; the matrix behaves exactly as the accepted baseline).
  tail_buffer_.configure(config_.tail.length);
  led_driver_begin_tail(config_.tail);
  frame_buffer_.set_orientation(config_.orientation);

  // Safe known boot state (§12.6): one forced blank frame at the configured
  // brightness — heritage `FastLED.clear(true)` in setup().
  led_driver_set_brightness(brightness_.hardware_scale());
  led_driver_clear();
  led_driver_show();
  throttle_.did_show(now_ms);
  machine_.clear_dirty();
  brightness_.clear_dirty();

  // WiFi station bring-up (§18 config, §12 reconnect-with-backoff). The device
  // stays fully functional as a renderer without WiFi (ADR-001); the API is
  // simply offline until a link exists. Power settings COPY/ADAPT the heritage
  // `eq_pi_link.cpp` bring-up: this devkit's supply cannot sustain default-TX
  // bursts (heritage disabled the brownout detector for the same reason), so
  // cap TX power and disable power-save or the link dies silently mid-burst.
  // First association is deferred to the backoff deadline (heritage boots with
  // a 2 s connect delay; the 8 s floor serves the same purpose), and heritage
  // pairs EVERY begin with a preceding disconnect — retried begins without it
  // never recover on this core/hardware combination.
  // [D-L008-1 + reconnect pairing — LED-L008 hardware campaign fixes, protected.]
  wifi_configured_ = config_.wifi_ssid[0] != '\0';
  if (wifi_configured_) {
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
#ifdef LED_WALL_ENABLE_DS4
    // The Bluepad32 BT coexistence stack aborts if Wi-Fi modem sleep is
    // disabled. Keep the V5 non-BT power setting unchanged in esp32_eq.
    WiFi.setSleep(WIFI_PS_MIN_MODEM);
#else
    WiFi.setSleep(WIFI_PS_NONE);
#endif
    WiFi.setAutoReconnect(true);
    WiFi.setTxPower(WIFI_POWER_11dBm);
    wifi_connecting_ = true;
    last_wifi_attempt_ms_ = now_ms;
  } else {
    add_warning(&warnings_, "wifi not configured; semantic API offline (config wifi.ssid)");
  }

#ifdef LED_WALL_ENABLE_DS4
  // LED-L010: DualShock 4 provider (esp32_eq_ds4 build only). Scanning is
  // automatic; the pure core maps callbacks into player-attributed events.
  ds4::begin(config_.ds4_enabled, &input_queue_);
#endif
}

void Runtime::tick(uint32_t now_ms) {
  maintain_wifi_(now_ms);

  // LED-L010: DS4 controller callbacks → player-attributed events (ds4 build).
#ifdef LED_WALL_ENABLE_DS4
  const uint8_t phone_mask = static_cast<uint8_t>(
      (http_.input_slot_claimed(1) ? 1u : 0u) |
      (http_.input_slot_claimed(2) ? 2u : 0u));
  const bool wifi_stable = !wifi_configured_ ||
      (wifi_online_since_ms_ != 0 && now_ms - wifi_online_since_ms_ >= 3000u);
  ds4::tick(now_ms, wifi_stable, phone_mask);
  const uint8_t ds4_lost = ds4::take_lost_mask();
  if (ds4_lost != 0) {
    input_queue_.discard_ds4(ds4_lost);
    if (game_host_.pause_for_disconnect()) force_render_ = true;
  }
#endif

  // LED-L009: physical inputs → normalized events (inert until enabled).
  gpio_inputs_.poll(now_ms, &input_queue_);

  // Drain the unified input queue: menu, games, timer and standby all speak
  // the same event language (reconstruction brief §10); LED-L010 events carry
  // a player slot (games route by slot; single-user consumers ignore it).
  PlayerEvent pe{};
  while (input_queue_.pop(&pe)) {
    if (http_.input_slot_claimed(pe.player) && pe.source != InputSource::PhoneSession)
      continue;  // one owner per player slot, including DS4/GPIO producers
    dispatch_input_(pe, now_ms);
  }

  // Device-owned countdown: runs with zero clients connected (brief §6).
  const bool timer_deadline = timer_.deadline_passed(now_ms);
  timer_.tick(now_ms);
  if (timer_deadline && timer_.state() == CountdownTimer::State::Finished) {
    events_.push(EventKind::TimerDone);
    if (sound_ != nullptr) {
      sound_->play(SoundCue::TimerDone);
    }
    // The finale may take foreground only while the timer still owns it.
    // A timer running behind a game, menu, standby, or another scene sends
    // its completion event without replacing that user's current program.
    if (machine_.snapshot(now_ms).current_mode == ModeKind::Timer &&
        config_.tail.enabled) {
      SceneSpec finale;
      if (scene_preset("countdown_finale", &finale)) play_scene(finale, now_ms);
    }
  }

  // A game never continues behind another foreground program. HTTP commands
  // can replace the mode at the end of the previous tick; close its game
  // before the next physics step, including when Pixel or Party took over.
  if (game_host_.active() && machine_.snapshot(now_ms).base_mode != ModeKind::Game) {
    game_host_.exit();
    input_queue_.clear();
  }
  // Games advance on their fixed cadence while running.
  game_host_.update(now_ms);
  sync_game_score_();
  sound_scheduler_.tick(now_ms);

  // Scene ownership is bounded to the exact base program the timeline last
  // applied. A user command that changes foreground cancels Party immediately.
  if (scene_.active()) {
    const ModeSnapshot owner = machine_.snapshot(now_ms);
    if (owner.base_mode != scene_expected_mode_ ||
        owner.base_started_ms != scene_expected_started_ms_) {
      scene_.stop();
    } else {
      const bool changed = scene_.tick(now_ms);
      if (!scene_.active()) machine_.clear(now_ms);
      else if (changed) apply_scene_step_(now_ms);
    }
  }

  // Visualizer decay/fail-safe.
  spectrum_.tick(now_ms);

  // Notification payload liveness follows the transient (§11 expiry model).
  const ModeSnapshot snap = machine_.snapshot(now_ms);
  const bool fireworks = (snap.current_mode == ModeKind::Scene ||
                          snap.current_mode == ModeKind::Animation) &&
                         snap.current_zone_target == ZoneTarget::Both &&
                         std::strcmp(snap.current_name, "fireworks") == 0;
  if (fireworks) {
    const uint32_t elapsed = now_ms - snap.current_started_ms;
    const uint32_t cycle = elapsed / 4800u;
    const uint32_t phase = elapsed % 4800u;
    if (!fireworks_active_ || fireworks_started_ms_ != snap.current_started_ms ||
        fireworks_cycle_ != cycle) {
      fireworks_active_ = true;
      fireworks_started_ms_ = snap.current_started_ms;
      fireworks_cycle_ = cycle;
      fireworks_exploded_ = false;
      if (sound_ != nullptr) sound_->play(SoundCue::Launch);
    }
    if (!fireworks_exploded_ && phase >= 2750u) {
      fireworks_exploded_ = true;
      if (sound_ != nullptr) sound_->play(SoundCue::Explosion);
    }
  } else fireworks_active_ = false;
  notify_.active = snap.transient_active && snap.transient_kind == ModeKind::Notification;
  menu_.brightness_mirror = brightness_.level();
  menu_.standby_mirror = snap.base_mode == ModeKind::Standby;

  // LED-L007: the current semantic program determines the frame. Time-varying
  // programs (scrolling text, animated scenes/animations/expressions) mark
  // the throttle dirty every tick so they re-render through the accepted
  // 50 ms floor; static programs render only on machine/brightness changes.
  const renderer::RenderInput input = renderer_input_(snap, now_ms);
  // LED-L009 repair cycle 2 (V-L009-1): text programs re-render only when their
  // frame content actually changed since the last SHOWN frame — a marquee at
  // >50 ms/px advances less than one pixel per throttle frame, and re-emitting
  // byte-identical full-strip frames several times per pixel step only wastes
  // the loop and stresses the panel's recorded latch marginality (O-L008-2).
  // Visuals are identical (sub-pixel redraws do not change the frame); games,
  // timer, menu, expressions and the visualizer keep the continuous cadence.
  bool content_changed = renderer::program_is_time_varying(input);
  if (content_changed && input.kind == ModeKind::Text && input.text != nullptr) {
    content_changed = input.text->tail_mode == TextTailMode::RainbowFlow ||
                      input.text->tail_mode == TextTailMode::Pulse ||
                      input.text->tail_mode == TextTailMode::Progress ||
                      input.text->tail_mode == TextTailMode::TextSynced ||
                      renderer::text_frame_content_changed(*input.text, now_ms, last_text_shown_ms_);
  }

  // §12 frame-request model (D-L008-3/D-L008-5 repair): every content change
  // (machine/brightness dirty) requests exactly one frame through the accepted
  // 50 ms floor; time-varying programs request continuously (20 fps max).
  // Before the repair the gate also required the throttle's own dirty flag,
  // which static updates never set — rapid static updates (countdown driver,
  // repeated text.show) rendered nothing beyond their first lucky frame.
  if (machine_.dirty() || brightness_.dirty() || content_changed || force_render_) {
    throttle_.mark_dirty();
  }

  bool frame_shown = false;
  // D-L008-3 final form: the throttle dirty flag belongs in the render gate —
  // time-varying programs set it every tick (continuous 50 ms re-render) and
  // content changes set machine/brightness dirty (one-shot re-render).
  // LED-L009: force_render_ adds one-shot render requests from the physical
  // input paths (timer state flips do not touch the machine/brightness).
  const bool visual_dirty =
      machine_.dirty() || brightness_.dirty() || throttle_.dirty() || force_render_;
  if (visual_dirty && throttle_.take_frame(now_ms)) {
    render_frame_(input);
    machine_.clear_dirty();
    brightness_.clear_dirty();
    throttle_.did_show(now_ms);
    frame_shown = true;
    last_text_shown_ms_ = now_ms;  // V-L009-1: cadence reference for text
  }
  force_render_ = false;

  // §9 text.done (LED-L007): render-completion event, once per program
  // activation (static text: first shown frame; scrolling: full traversal;
  // LED-L009 word-by-word one-shot: last word's dwell elapsed).
  if (snap.current_mode == ModeKind::Text && input.text != nullptr &&
      !input.text->done_reported &&
      renderer::text_program_complete(*input.text, now_ms, frame_shown)) {
    input.text->done_reported = true;
    events_.push(EventKind::TextDone);
  }

  if (servers_started_) {
    http_.tick();
    const uint8_t lost_players = http_.take_input_loss_mask();
    if (lost_players != 0) {
      input_queue_.discard_phone(lost_players);
      if (game_host_.pause_for_disconnect()) force_render_ = true;
    }
    ws_.tick();
    drain_events_();
  }
}

void Runtime::maintain_wifi_(uint32_t now_ms) {
  if (!wifi_configured_) {
    return;
  }
  const bool connected = WiFi.status() == WL_CONNECTED;
  if (connected) {
    if (wifi_online_since_ms_ == 0) wifi_online_since_ms_ = now_ms;
  } else {
    wifi_online_since_ms_ = 0;
  }
  if (connected && !servers_started_) {
    http_.begin(static_cast<uint16_t>(config_.http_port), &dispatcher_, identity_provider,
                state_provider, pixel_session_provider, pixel_frame_provider,
                scene_play_provider, scene_stop_provider, &input_queue_,
                sound_mute_provider);
    ws_.begin(static_cast<uint16_t>(config_.ws_port));
    servers_started_ = true;
    wifi_connecting_ = false;
    return;
  }
  if (!connected && now_ms - last_wifi_attempt_ms_ >= kWifiReconnectBackoffMs) {
    // Heritage 8 s reconnect backoff (§12); wrap-safe via unsigned subtraction.
    // Heritage startWifiConnect() pairs a disconnect with every begin (see
    // begin() note) — without it, retried association attempts never recover.
    // [D-L008-1 reconnect fix — protected.]
    WiFi.disconnect(false, false);
    WiFi.begin(config_.wifi_ssid, config_.wifi_password);
    wifi_connecting_ = true;
    last_wifi_attempt_ms_ = now_ms;
  }
}

// ---------------------------------------------------------- input routing ----
void Runtime::enter_menu_(uint32_t now_ms) {
  // Entering the menu pauses a running game (menu is the foreground mode).
  games::Game* g = game_host_.current_game();
  if (g != nullptr && g->phase() == games::GamePhase::Running) {
    g->set_phase(games::GamePhase::Paused);
  }
  menu_.open(now_ms);
  machine_.apply_persistent(ModeKind::Menu, "", now_ms);  // wakes standby too
}

void Runtime::exit_menu_(uint32_t now_ms) {
  menu_.close();
  // No-stack model (§11): the menu replaced the previous base, exit lands on
  // idle (documented; games keep their paused state and can be re-entered).
  if (machine_.snapshot(now_ms).base_mode == ModeKind::Menu) {
    machine_.replace_base(ModeKind::Idle, "", now_ms);
  }
}

void Runtime::apply_menu_action_(const MenuAction& action, uint32_t now_ms) {
  switch (action.kind) {
    case MenuAction::Kind::None:
      break;
    case MenuAction::Kind::EnterTimer:
      if (timer_.duration_ms() == 0) {
        timer_.arm(kMenuDefaultTimerDurationMs);
      }
      machine_.apply_persistent(ModeKind::Timer, "", now_ms);
      break;
    case MenuAction::Kind::SelectAnimation:
      if (action.index < kAnimationCount) {
        machine_.apply_persistent(ModeKind::Animation, kAnimationNames[action.index], now_ms);
      }
      break;
    case MenuAction::Kind::SelectExpression:
      if (action.index < kExpressionCount) {
        machine_.apply_persistent(ModeKind::Expression, kExpressionNames[action.index], now_ms);
      }
      break;
    case MenuAction::Kind::SelectGame:
      if (action.index < kGameCount) {
        if (game_min_players(kGameNames[action.index]) == 2)
          game_host_.set_players(2);
        if (game_host_.start_by_index(action.index, now_ms))
          machine_.apply_persistent(ModeKind::Game, kGameNames[action.index], now_ms);
      }
      break;
    case MenuAction::Kind::SetBrightness:
      brightness_.set_level(action.level);
      break;
    case MenuAction::Kind::ToggleStandby:
      if (machine_.snapshot(now_ms).base_mode == ModeKind::Standby) {
        machine_.replace_base(ModeKind::Idle, "", now_ms);  // wake
      } else {
        machine_.apply_persistent(ModeKind::Standby, "", now_ms);
      }
      break;
    case MenuAction::Kind::Exit:
      exit_menu_(now_ms);
      break;
  }
}

void Runtime::dispatch_input_(const PlayerEvent& pe, uint32_t now_ms) {
  const InputEvent event = pe.event;
  const uint8_t player = (pe.player == kPlayer2) ? kPlayer2 : kPlayer1;
  const ModeSnapshot snap = machine_.snapshot(now_ms);

  // Notification transient: Confirm/Back dismisses; other keys are swallowed
  // so games/menus never act on input aimed at the notification.
  if (snap.transient_active && snap.transient_kind == ModeKind::Notification) {
    if (event == InputEvent::Confirm || event == InputEvent::Back) {
      machine_.cancel_transient(now_ms);
      notify_.active = false;
    }
    return;
  }

  if (event == InputEvent::Home) {
    enter_menu_(now_ms);  // global: menu everywhere (wakes standby)
    return;
  }

  if (menu_.active()) {
    MenuAction action;
    menu_.handle_input(event, menu_actions_device(), &action);
    apply_menu_action_(action, now_ms);
    return;
  }

  switch (snap.current_mode) {
    case ModeKind::Standby:
      // Wake on any action key (brief §21); brightness/other keys stay inert.
      if (event == InputEvent::Confirm || event == InputEvent::StartPause ||
          event == InputEvent::Back) {
        machine_.replace_base(ModeKind::Idle, "", now_ms);
      }
      break;
    case ModeKind::Timer: {
      // One-button countdown semantics (brief §8):
      //   Confirm/StartPause → start → pause → resume;
      //   Back → leave the timer display (countdown keeps running);
      //   Left/Right → arm duration −/+ 5 s while not running.
      if (event == InputEvent::Confirm || event == InputEvent::StartPause) {
        switch (timer_.state()) {
          case CountdownTimer::State::Idle:
            timer_.start(now_ms);
            break;
          case CountdownTimer::State::Running:
            timer_.pause(now_ms);
            break;
          case CountdownTimer::State::Paused:
            timer_.resume(now_ms);
            break;
          case CountdownTimer::State::Finished:
            timer_.reset(now_ms);  // finished → press re-arms
            break;
        }
        force_render_ = true;
      } else if (event == InputEvent::Back) {
        machine_.replace_base(ModeKind::Idle, "", now_ms);
      } else if (event == InputEvent::Left || event == InputEvent::Right) {
        if (timer_.state() == CountdownTimer::State::Idle) {
          const int32_t delta = (event == InputEvent::Right) ? 5000 : -5000;
          const int32_t next = static_cast<int32_t>(timer_.duration_ms()) + delta;
          if (next >= 1000 && next <= static_cast<int32_t>(CountdownTimer::kMaxDurationMs)) {
            timer_.arm(static_cast<uint32_t>(next));
            force_render_ = true;
          }
        }
      }
      break;
    }
    case ModeKind::Game: {
      const bool uses_action_b = std::strcmp(game_host_.name(), "light_race") == 0 ||
                                 std::strcmp(game_host_.name(), "color_chain") == 0;
      if (event == InputEvent::Back && !uses_action_b) {
        game_host_.exit();
        machine_.replace_base(ModeKind::Idle, "", now_ms);
        return;
      }
      game_host_.handle(player, event == InputEvent::Back && uses_action_b ?
                               InputEvent::ActionB : event, now_ms);
      break;
    }
    case ModeKind::Menu:
      // menu_.active() branch above covers the normal path; a stale Menu base
      // without an active menu state falls through safely.
      break;
    default:
      break;  // text/scene/animation/expression/spectrum: no input bindings
  }
}

MenuActions& Runtime::menu_actions_device() {
  static RuntimeMenuActions menu_actions;  // single instance (no heap churn)
  menu_actions.runtime = this;
  return menu_actions;
}

void Runtime::drain_events_() {
  if (events_.pending() == 0) {
    return;
  }
  // Coalesce pending events into one state broadcast per loop iteration
  // (§9 bounded broadcast). The event kinds distinguish payload envelopes.
  Event drained[kMaxPendingEvents];
  const uint8_t taken = events_.drain(drained, kMaxPendingEvents);
  bool broadcast_state = false;
  bool broadcast_mode_changed = false;
  bool broadcast_transient_expired = false;
  bool broadcast_text_done = false;
  bool broadcast_timer_done = false;
  for (uint8_t i = 0; i < taken; ++i) {
    switch (drained[i].kind) {
      case EventKind::StateChanged:
        broadcast_state = true;
        break;
      case EventKind::ModeChanged:
        broadcast_mode_changed = true;
        break;
      case EventKind::TransientExpired:
        broadcast_transient_expired = true;
        break;
      case EventKind::TextDone:
        broadcast_text_done = true;
        break;
      case EventKind::TimerDone:
        broadcast_timer_done = true;
        break;
    }
  }
  if (broadcast_state) {
    JsonDocument doc;
    doc["event"] = "state.changed";
    serialize_state(state(millis()), identity(), doc["state"]);
    String out;
    serializeJson(doc, out);
    ws_.broadcast(out.c_str());
  }
  if (broadcast_mode_changed) {
    JsonDocument doc;
    doc["event"] = "mode.changed";
    doc["currentMode"] = mode_name(state(millis()).current_mode);
    String out;
    serializeJson(doc, out);
    ws_.broadcast(out.c_str());
  }
  if (broadcast_transient_expired) {
    JsonDocument doc;
    doc["event"] = "transient.expired";
    String out;
    serializeJson(doc, out);
    ws_.broadcast(out.c_str());
  }
  if (broadcast_text_done) {
    JsonDocument doc;
    doc["event"] = "text.done";  // §9 render-completion event (LED-L007)
    String out;
    serializeJson(doc, out);
    ws_.broadcast(out.c_str());
  }
  if (broadcast_timer_done) {
    JsonDocument doc;
    doc["event"] = "timer.done";  // LED-L009: countdown reached zero
    String out;
    serializeJson(doc, out);
    ws_.broadcast(out.c_str());
  }
}

renderer::RenderInput Runtime::renderer_input_(const ModeSnapshot& snap, uint32_t now_ms) {
  renderer::RenderInput in;
  in.kind = snap.current_mode;
  in.name = snap.current_name;
  in.started_ms = snap.current_started_ms;
  in.now_ms = now_ms;
  in.target = snap.current_zone_target;  // LED-L010: commanded zone
  in.pixel = &pixel_canvas_;  // Text Custom tail also reads the last device snapshot.
  // §4: the text program follows the current mode slot (transient text
  // overrides base text; expiry falls back without mutation).
  if (in.kind == ModeKind::Text) {
    in.text = (snap.transient_active && snap.transient_kind == ModeKind::Text)
                  ? &text_.transient
                  : &text_.base;
  }
  // LED-L009 subsystem views (read-only for the renderer).
  switch (in.kind) {
    case ModeKind::Timer:
      in.timer = &timer_;
      break;
    case ModeKind::Spectrum:
      in.spectrum = &spectrum_;
      break;
    case ModeKind::Menu:
      in.menu = &menu_;
      break;
    case ModeKind::Game:
      in.game = game_host_.current_game();
      break;
    case ModeKind::Notification:
      in.notify = &notify_;
      break;
    case ModeKind::Paint:
      in.pixel = &pixel_canvas_;
      break;
    default:
      break;
  }
  return in;
}

// LED-L007: render the active semantic program into the render framebuffer and
// hand it to the LED driver below the brightness pipeline. LED-L010: the tail
// surface renders beside the matrix when the zone is enabled (nullptr =
// disabled — the renderer skips every tail write, matrix behavior unchanged).
void Runtime::render_frame_(const renderer::RenderInput& input) {
  led_driver_set_brightness(brightness_.hardware_scale());
  TailFramebuffer* tail = config_.tail.enabled ? &tail_buffer_ : nullptr;
  renderer::RenderInput frame_input = input;
  if (++frame_sequence_ == 0) ++frame_sequence_;
  frame_input.frame_id = frame_sequence_;
  renderer::render(frame_buffer_, tail, frame_input);
  if (tail != nullptr && tail_point_index_ >= 0) {
    // A fixed, single white emitter until OFF or another command. Reuse the
    // existing bounded TailFramebuffer and physical driver mapping.
    frame_buffer_.clear();
    tail->clear();
    tail->set(static_cast<uint16_t>(tail_point_index_), Rgb{255, 255, 255});
  }
  const OutputScale output = output_limit(frame_buffer_.data(), LED_WALL_LED_COUNT,
                                           tail != nullptr ? tail->data() : nullptr,
                                           tail != nullptr ? tail->length() : 0,
                                           brightness_.hardware_scale());
  led_driver_blit(frame_buffer_.data(), output.matrix);
  if (tail != nullptr) {
    led_driver_blit_tail(tail->data(), tail->length(), output.tail);
  }
  led_driver_show();
}

uint32_t Runtime::begin_pixel_session() { return pixel_canvas_.begin_session(); }

PixelCanvas::ApplyResult Runtime::apply_pixel_frame(uint32_t session, uint32_t revision,
                                                      const char* pixels, size_t length,
                                                      bool live, uint32_t now_ms) {
  if (!config_.tail.enabled || config_.tail.length != PixelCanvas::kTailCells) {
    return PixelCanvas::ApplyResult::TailUnavailable;
  }
  const auto result = pixel_canvas_.apply(session, revision, pixels, length, live, now_ms);
  if (result == PixelCanvas::ApplyResult::Accepted) {
    sound_scheduler_.clear();
    tail_point_index_ = -1;
    machine_.apply_persistent(ModeKind::Paint, "", now_ms);
    force_render_ = true;
  }
  return result;
}

bool Runtime::play_scene(const SceneSpec& spec, uint32_t now_ms) {
  const ModeSnapshot snap = machine_.snapshot(now_ms);
  if (snap.current_mode == ModeKind::Game || snap.current_mode == ModeKind::Standby ||
      snap.current_mode == ModeKind::Menu) return false;
  // Validate before replacing a currently playing scene.
  SceneTimeline candidate;
  if (!candidate.start(spec, now_ms)) return false;
  scene_ = candidate;
  sound_scheduler_.clear();
  apply_scene_step_(now_ms);
  force_render_ = true;
  return true;
}

void Runtime::stop_scene(uint32_t now_ms) {
  if (!scene_.active()) return;
  scene_.stop();
  sound_scheduler_.clear();
  machine_.clear(now_ms);
  force_render_ = true;
}

void Runtime::apply_scene_step_(uint32_t now_ms) {
  const SceneStep* step = scene_.current();
  if (step == nullptr) return;
  if (step->kind == SceneStepKind::Text) {
    TextProgram program;
    std::strncpy(program.current, step->content, kTextMaxLen - 1);
    if (!text::fits_static(program.current)) {
      program.mode = TextMode::Marquee;
      program.scrolling = true;
    }
    program.started_ms = now_ms;
    // Party text occupies both regions for the same bounded scene step.
    program.duration_ms = step->duration_ms;
    program.tail_mode = TextTailMode::Progress;
    if (std::strcmp(scene_.name(), "welcome") == 0 && pixel_canvas_.active())
      program.tail_mode = TextTailMode::Custom;
    text_.base = program;
    machine_.apply_persistent(ModeKind::Text, "", now_ms);
    scene_expected_mode_ = ModeKind::Text;
  } else if (step->kind == SceneStepKind::Effect) {
    const ZoneTarget target = animation_default_target(step->content);
    // A tail-only program cannot own the disabled branch.
    if (target == ZoneTarget::Tail && !config_.tail.enabled) {
      machine_.apply_persistent(ModeKind::Idle, "", now_ms);
      scene_expected_mode_ = ModeKind::Idle;
    } else {
      machine_.apply_persistent(ModeKind::Scene, step->content, now_ms, target);
      scene_expected_mode_ = ModeKind::Scene;
    }
  } else if (step->kind == SceneStepKind::GameReady) {
    const bool needs_tail = game_min_lane(step->content) > 0;
    const bool available = !needs_tail ||
        (config_.tail.enabled && config_.tail.length >= game_min_lane(step->content));
    game_host_.set_players(game_min_players(step->content));
    if (available && game_host_.start_by_name(step->content, now_ms)) {
      machine_.apply_persistent(ModeKind::Game, step->content, now_ms);
      scene_.stop();  // leave the game Ready; no synthetic player input
      scene_expected_mode_ = ModeKind::Game;
    } else {
      machine_.apply_persistent(ModeKind::Idle, "", now_ms);
      scene_.stop();
      scene_expected_mode_ = ModeKind::Idle;
    }
  } else {
    machine_.apply_persistent(ModeKind::Idle, "", now_ms);
    scene_expected_mode_ = ModeKind::Idle;
  }
  scene_expected_started_ms_ = now_ms;
  force_render_ = true;
}

void Runtime::sync_game_score_() {
  if (!game_host_.active()) {
    score_key_[0] = '\0';
    game_high_score_available_ = false;
    score_recorded_ = false;
    return;
  }
  char key[16];
  game_score_key(game_host_.name(), game_host_.active_difficulty(),
                 game_host_.active_players(), key);
  if (std::strcmp(key, score_key_) != 0) {
    std::strncpy(score_key_, key, sizeof(score_key_) - 1);
    score_key_[sizeof(score_key_) - 1] = '\0';
    score_recorded_ = false;
    game_high_score_available_ = false;
    Preferences prefs;
    if (prefs.begin("lw_scores", true)) {
      game_high_score_available_ = prefs.isKey(score_key_);
      if (game_high_score_available_) game_high_score_ = prefs.getUInt(score_key_, 0);
      prefs.end();
    }
  }
  if (game_host_.phase() != games::GamePhase::Over) {
    score_recorded_ = false;
    return;
  }
  if (score_recorded_) return;
  score_recorded_ = true;
  // A 2P match records the best player result in its separate mode key;
  // neither physical slot gets preferential treatment in the record.
  const uint32_t p1 = game_host_.player_score(1);
  const uint32_t p2 = game_host_.player_score(2);
  const uint32_t score = game_host_.active_players() == 2 && p2 > p1 ? p2 : p1;
  const bool lower_wins = std::strcmp(game_host_.name(), "reaction") == 0;
  const bool improved = !game_high_score_available_ ||
                        (lower_wins ? score < game_high_score_ : score > game_high_score_);
  if (!improved) return;
  Preferences prefs;
  if (!prefs.begin("lw_scores", false)) return;
  const bool saved = prefs.putUInt(score_key_, score) == sizeof(uint32_t);
  prefs.end();
  if (saved) {
    game_high_score_ = score;
    game_high_score_available_ = true;
  }
}

PublicState Runtime::state(uint32_t now_ms) const {
  DeviceStateViews views;
  views.timer_state = timer_.state();
  views.timer_duration_ms = timer_.duration_ms();
  views.timer_remaining_ms = timer_.remaining_ms(now_ms);
  views.menu_active = menu_.active();
  views.game_active = game_host_.active();
  views.game_name = game_host_.name();
  views.game_phase = games::game_phase_name(game_host_.phase());
  views.game_score = game_host_.player_score(1);
  views.game_score_p2 = game_host_.player_score(2);
  views.game_winner = game_host_.winner();
  views.game_high_score = game_high_score_;
  views.game_high_score_available = game_high_score_available_;
  views.game_difficulty = games::game_difficulty_name(game_host_.difficulty());
  views.game_players = game_host_.active_players();
  views.standby_on = machine_.snapshot(now_ms).base_mode == ModeKind::Standby;
  views.notification_active = notify_.active;
  views.notification_high = notify_.high_priority;
  views.notification_text = notify_.text;
  views.visualizer_active = spectrum_.active(now_ms);
  views.sound_muted = sound_scheduler_.muted();
  // LED-L010 views: tail zone truth + the current program's commanded target.
  views.tail_enabled = config_.tail.enabled;
  views.tail_length = config_.tail.length;
  views.tail_reverse = config_.tail.reverse;
  views.tail_setup_index = tail_point_index_;
  const ModeSnapshot snap = machine_.snapshot(now_ms);
  // Repair cycle 1 (M-L010-6): the reported target follows the applicable
  // scene/animation program (current slot first, else base slot) instead of
  // only the current slot — a notification over a tail animation now reports
  // the animation's target, matching the contract's any-slot rule.
  const char* zone = reported_zone_target(snap);
  if (zone != nullptr) {
    views.animation_target = zone;
  }
  if (config_.tail.enabled &&
      (snap.current_mode == ModeKind::Scene || snap.current_mode == ModeKind::Animation) &&
      std::strcmp(snap.current_name, "scanner") == 0 &&
      snap.current_zone_target == ZoneTarget::Tail) {
    views.tail_scan_index = static_cast<int16_t>(effects::tail_scanner_position(
        config_.tail.length, now_ms - snap.current_started_ms));
  }
#ifdef LED_WALL_ENABLE_DS4
  const ds4::StateView dv = ds4::view();
  views.ds4_supported = true;
  views.ds4_enabled = dv.enabled;
  views.ds4_state = ds4::state_name(dv.state);
  views.ds4_players = dv.players;
  views.ds4_battery_p1 = dv.battery_p1;
  views.ds4_battery_p2 = dv.battery_p2;
  views.ds4_ready = dv.ready;
  views.ds4_assigned_mask = dv.assigned_mask;
  views.ds4_input_events = dv.input_events;
#endif

  PublicState s = build_public_state(machine_, brightness_, text_, warnings_, now_ms,
                                     (now_ms - boot_ms_) / 1000u, views);
  std::snprintf(s.device_id, sizeof(s.device_id), "%s", resolve_device_id());
  // Same kFwVersion as the identity handshake — the §4 state snapshot must
  // not report a stale default (divergence found during LED-L007).
  std::snprintf(s.fw_version, sizeof(s.fw_version), "%s", kFwVersion);
  s.clients_count = const_cast<WsApi&>(ws_).clients_count();
  return s;
}

Identity Runtime::identity() const {
  return build_identity(resolve_device_id(), kFwVersion, /*spectrum_available=*/false);
}

}  // namespace ledwall
