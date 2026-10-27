#pragma once

#include <cstdint>

#include "input/input_events.h"

namespace ledwall {

// LED-L009: the bridge between the pure semantic dispatcher and the device
// subsystems (timer, input router, menu, games host, visualizer, notification
// payload, standby). The dispatcher validates and decides; services execute.
// The runtime implements this once; host tests implement fakes — the same
// seam that keeps the dispatcher host-testable.
class DeviceServices {
 public:
  virtual ~DeviceServices() = default;

  // Countdown/timer engine (pure device state — no client needed).
  virtual bool timer_set(uint32_t duration_ms) = 0;
  virtual bool timer_start(uint32_t now_ms) = 0;
  virtual bool timer_pause(uint32_t now_ms) = 0;
  virtual bool timer_resume(uint32_t now_ms) = 0;
  virtual void timer_reset(uint32_t now_ms) = 0;

  // Unified input (API provider): one normalized event from a client.
  virtual bool input_send(InputEvent event) = 0;
  // LED-L010: player-attributed event. The default routes as P1 — legacy
  // host-test fakes keep working unchanged.
  virtual bool input_send_player(uint8_t player, InputEvent event) {
    (void)player;
    return input_send(event);
  }

  // Local pixel menu.
  virtual bool menu_enter(uint32_t now_ms) = 0;
  virtual bool menu_exit() = 0;

  // Games host.
  virtual bool game_start(const char* name, uint32_t now_ms) = 0;
  virtual bool game_command(const char* action, uint32_t now_ms) = 0;
  virtual void game_exit() = 0;
  // V-L009-3: engine-enforced difficulty (persists; applies at next restart).
  virtual void game_set_difficulty(int difficulty) = 0;
  // LED-L010: engine-enforced player mode 1|2 (persists; applies at next
  // start/restart). Default no-op — legacy fakes keep compiling.
  virtual void game_set_players(int players) { (void)players; }

  // LED-L010: tail zone availability (driver/config truth). The dispatcher
  // rejects tail-target commands with ledUnavailable when the tail is
  // disabled; the default (false) fails closed for minimal host fakes.
  virtual bool tail_enabled() const { return false; }
  // Repair cycle 1 (M-L010-2/3): configured tail lane length (logical cells).
  // The dispatcher rejects tail lane games whose registry minimum exceeds it.
  // The default is the documented placeholder lane so minimal host fakes keep
  // the accepted behavior.
  virtual uint16_t tail_length() const { return 60; }

  // LED-L010 bounded physical count diagnostic. Callers choose only an index;
  // the runtime owns the fixed white pixel and output bounds.
  virtual bool tail_point_set(uint16_t index) { (void)index; return false; }
  virtual void tail_point_off() {}
  virtual bool tail_point_active() const { return false; }

  // Visualizer input.
  virtual bool visualizer_feed(const uint8_t* bands, uint8_t count, uint32_t now_ms) = 0;
  virtual void visualizer_clear() = 0;

  // Notification payload store (the transient itself is mode-machine state).
  virtual void notify_store(const char* text, bool high_priority) = 0;

  // Sound cue seam (null sink on this hardware).
  virtual void play_cue(int cue) = 0;
};

}  // namespace ledwall
