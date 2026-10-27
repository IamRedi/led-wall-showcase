#pragma once

#include <ArduinoJson.h>
#include <WebServer.h>

#include <cstdint>

#include "api/command.h"
#include "api/identity.h"
#include "core/public_state.h"
#include "pixel/pixel_canvas.h"
#include "input/input_sessions.h"
#include "scene/scene_timeline.h"

namespace ledwall {

// Semantic HTTP API (frozen spec §9/§10): command/query endpoints served with
// the Arduino-ESP32 WebServer. Requests are translated into pure
// SemanticCommands and executed by the CommandDispatcher — the handler never
// touches the renderer (§12: network callbacks never do long LED work).
//
// Routes (documented in docs/api/CONTRACT.md):
//   GET  /                        embedded dashboard (PROGMEM static)
//   GET  /state                   §4 snapshot (+ LED-L009 subsystem truth)
//   GET  /identity                §10 handshake (+ LED-L009 capabilities)
//   GET  /registry                LED-L009: current registries (discovery)
//   POST /display/text/show       {text, scroll?, speedMsPerPx?, durationMs?,
//                                 mode?, direction?, loop?, wordDwellMs?,
//                                 style?, color?, bgColor?}
//   POST /display/text/clear      {}
//   POST /display/scene/set       {scene, durationMs?}
//   POST /display/animation/set   {animation, durationMs?}
//   POST /display/expression/set  {expression}
//   POST /display/clear           {}
//   POST /display/brightness/set  {level}
//   POST /display/tail/point      {index:0..199} (LED-L010 bounded setup)
//   POST /display/tail/off        {}             (LED-L010 bounded setup)
//   POST /display/timer/set       {durationMs}              (LED-L009)
//   POST /display/timer/start|pause|resume|reset|stop  {}    (LED-L009)
//   POST /display/input           {input}                   (LED-L009)
//   POST /display/menu/set        {action: enter|exit}      (LED-L009)
//   POST /display/game/set        {game}                    (LED-L009)
//   POST /display/game/command    {action}                  (LED-L009)
//   POST /display/visualizer/start|stop  {}                 (LED-L009)
//   POST /display/visualizer/data {bands: [30 ints]}        (LED-L009)
//   POST /display/notify          {text, durationMs?, priority?}  (LED-L009)
//   POST /display/standby/set     {on}                      (LED-L009)
class HttpApi {
 public:
  using IdentityFn = Identity (*)();
  using StateFn = PublicState (*)();
  using PixelSessionFn = uint32_t (*)();
  using PixelFrameFn = PixelCanvas::ApplyResult (*)(uint32_t, uint32_t, const char*,
                                                     size_t, bool, uint32_t);
  using ScenePlayFn = bool (*)(const SceneSpec&, uint32_t);
  using SceneStopFn = void (*)(uint32_t);
  using SoundMuteFn = void (*)(bool);

  // Provider callbacks return the current identity/state on the shared
  // monotonic timebase. `dispatcher` executes validated commands.
  void begin(uint16_t port, CommandDispatcher* dispatcher, IdentityFn identity_fn,
             StateFn state_fn, PixelSessionFn pixel_session_fn,
             PixelFrameFn pixel_frame_fn, ScenePlayFn scene_play_fn,
             SceneStopFn scene_stop_fn, InputEventQueue* input_queue,
             SoundMuteFn sound_mute_fn);
  void tick();  // polls the HTTP server (call once per main-loop iteration)
  bool running() const { return running_; }
  bool input_slot_claimed(uint8_t player) const { return input_sessions_.claimed(player); }
  uint8_t take_input_loss_mask() { return input_sessions_.take_lost_mask(); }

 private:
  void handle_command_(SemanticCommand::Capability capability);
  void serve_dashboard_();
  void send_registry_();
  bool parse_body_(JsonDocument& doc);
  void send_ok_(const CommandResponse& response);
  void send_error_(const CommandResponse& response);
  void send_state_();
  void send_identity_();
  void handle_pixel_session_();
  void handle_pixel_frame_();
  void handle_pixel_projects_();
  void handle_pixel_project_save_();
  void handle_pixel_project_load_();
  void handle_scene_play_();
  void handle_scene_projects_();
  void handle_scene_project_save_();
  void handle_scene_project_load_();
  void handle_input_session_();
  void handle_input_event_();
  void handle_input_heartbeat_();
  void handle_input_release_();

  // WebServer is not copyable/movable; constructed once at begin().
  WebServer* server_{nullptr};
  CommandDispatcher* dispatcher_{nullptr};
  IdentityFn identity_fn_{nullptr};
  StateFn state_fn_{nullptr};
  PixelSessionFn pixel_session_fn_{nullptr};
  PixelFrameFn pixel_frame_fn_{nullptr};
  ScenePlayFn scene_play_fn_{nullptr};
  SceneStopFn scene_stop_fn_{nullptr};
  InputSessions input_sessions_;
  InputEventQueue* input_queue_{nullptr};
  SoundMuteFn sound_mute_fn_{nullptr};
  bool running_{false};
  bool pixel_store_ready_{false};
};

}  // namespace ledwall
