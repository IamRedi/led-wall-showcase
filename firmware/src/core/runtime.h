#pragma once

#include <cstdint>

#include "api/command.h"
#include "api/device_services.h"
#include "api/http_server.h"
#include "api/identity.h"
#include "api/ws_server.h"
#include "core/config.h"
#include "core/config_store.h"
#include "core/mode_machine.h"
#include "core/public_state.h"
#include "games/game_host.h"
#include "input/gpio_input_provider.h"
#include "input/input_events.h"
#include "menu/local_menu.h"
#include "pixel/pixel_canvas.h"
#include "render/display_topology.h"
#include "render/frame_throttle.h"
#include "render/framebuffer.h"
#include "render/led_driver.h"
#include "render/renderer.h"
#include "scene/scene_timeline.h"
#include "sound/sound_sink.h"
#include "sound/sound_scheduler.h"
#include "timer/countdown.h"
#include "visualizer/spectrum_input.h"

namespace ledwall {

// Core runtime + semantic API wiring (frozen spec §4/§5/§9/§10/§11/§12/§18;
// LED-L009 experience reconstruction): boots into the configured safe default,
// runs the mode machine, applies the brightness ceiling, pushes frames through
// the heritage dirty-flag throttle, and serves the semantic HTTP/WS API once
// WiFi is up. Commands mutate state only — rendering stays behind the
// throttled main-loop path (§12).
// Single monotonic timebase: every timing path uses the same millis() value
// passed into tick()/state() (R-B02 contract).
//
// LED-L009 device-owned subsystems integrated here: countdown timer, unified
// input (GPIO provider + API provider → one event queue → menu/games/timer),
// local pixel menu, games host, visualizer input, notification payload, sound
// cue seam. The phone is never authoritative: everything here runs with zero
// clients connected.
class Runtime {
 public:
  void begin(uint32_t now_ms);
  void tick(uint32_t now_ms);

  PublicState state(uint32_t now_ms) const;
  Identity identity() const;
  uint32_t begin_pixel_session();
  PixelCanvas::ApplyResult apply_pixel_frame(uint32_t session, uint32_t revision,
                                               const char* pixels, size_t length,
                                               bool live, uint32_t now_ms);
  bool play_scene(const SceneSpec& spec, uint32_t now_ms);
  void stop_scene(uint32_t now_ms);
  void set_sound_muted(bool muted) { sound_scheduler_.set_muted(muted); }

 private:
  // DeviceServices bridge (dispatcher → subsystems), defined in runtime.cpp.
  class RuntimeServices;
  class RuntimeMenuActions;

  renderer::RenderInput renderer_input_(const ModeSnapshot& snap, uint32_t now_ms);
  void render_frame_(const renderer::RenderInput& input);
  void apply_scene_step_(uint32_t now_ms);
  void sync_game_score_();
  void maintain_wifi_(uint32_t now_ms);
  void drain_events_();
  void dispatch_input_(const PlayerEvent& pe, uint32_t now_ms);
  void apply_menu_action_(const MenuAction& action, uint32_t now_ms);
  void enter_menu_(uint32_t now_ms);
  void exit_menu_(uint32_t now_ms);
  MenuActions& menu_actions_device();

  AppConfig config_{};
  Brightness brightness_;
  ModeMachine machine_;
  TextState text_{};
  NotifyState notify_{};
  CommandDispatcher dispatcher_;
  EventSink events_;
  FrameThrottle throttle_;
  FrameBuffer frame_buffer_;  // LED-L007: render layer target (300 px)
  TailFramebuffer tail_buffer_;  // LED-L010: 1D tail surface (logical cells)
  uint32_t frame_sequence_{0};  // LED-L011: shared Matrix/Tail composition identity
  PixelCanvas pixel_canvas_;  // LED-L011: bounded product canvas, device-owned
  SceneTimeline scene_;
  ModeKind scene_expected_mode_{ModeKind::Idle};
  uint32_t scene_expected_started_ms_{0};
  char score_key_[16]{};
  uint32_t game_high_score_{0};
  bool game_high_score_available_{false};
  bool score_recorded_{false};
  uint32_t fireworks_started_ms_{0}, fireworks_cycle_{0};
  bool fireworks_active_{false}, fireworks_exploded_{false};
  int16_t tail_point_index_{-1};  // LED-L010 setup: -1=off; zero-based, at most 199
  Warnings warnings_{};
  uint32_t boot_ms_{0};

  // LED-L009 subsystems (device truth).
  CountdownTimer timer_;
  SpectrumInput spectrum_;
  MenuState menu_;
  games::GameHost game_host_;
  InputEventQueue input_queue_;
  GpioInputProvider gpio_inputs_;
  NullSoundSink null_sound_;
  SoundScheduler sound_scheduler_;
  SoundSink* sound_{nullptr};   // seam: replaceable by a future real sink
  RuntimeServices* services_{nullptr};

  bool wifi_configured_{false};
  bool wifi_connecting_{false};
  bool servers_started_{false};
  bool force_render_{false};  // LED-L009: one-shot render request from input paths
  uint32_t last_wifi_attempt_ms_{0};
  uint32_t wifi_online_since_ms_{0};
  // LED-L009 repair cycle 2 (V-L009-1): show-time of the last rendered frame —
  // the reference for the text pixel-cadence check (renderer::
  // text_frame_content_changed) that stops byte-identical marquee re-shows.
  uint32_t last_text_shown_ms_{0};
  static constexpr uint32_t kWifiReconnectBackoffMs = 8000;  // heritage pattern (§12)

  HttpApi http_;
  WsApi ws_;
};

}  // namespace ledwall
