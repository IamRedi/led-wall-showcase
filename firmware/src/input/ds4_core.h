#pragma once

#include <cstdint>

#include "input/input_events.h"

namespace ledwall {
namespace ds4 {

// LED-L010 DualShock 4 provider — PURE core (ADR-LED-L010 D5). Host-tested:
// the Sony button vocabulary exists ONLY here and in the device-only
// Bluepad32 glue; games/menu code sees normalized player-tagged events.
//
// Mapping (expansion brief §7):
//   Bluepad32 d-pad bitmask / left stick → up / down / left / right (repeat)
//   Cross   → Confirm (A)          Circle → Back (B)
//   Square  → Left (X)             Triangle → Right (Y)
//   OPTIONS → StartPause           SHARE → Home; PS is reserved
// Player slots: the first controller to connect becomes P1, the second P2
// (lifecycle below); identical mapping in both slots.

constexpr uint8_t kMaxControllers = 2;  // P1 + P2

// Normalized DS4 snapshot (the glue translates Bluepad32 values once).
struct Snapshot {
  // Bluepad32 bitmask: up=1, down=2, right=4, left=8; zero=released.
  uint8_t dpad{0};
  int16_t axis_x{0};  // Bluepad32 axis range -512..512
  int16_t axis_y{0};
  bool cross{false};
  bool circle{false};
  bool square{false};
  bool triangle{false};
  bool options{false};
  bool share{false};
  bool ps{false};
  bool l1{false};
  bool r1{false};
  // Normalized battery 0..100 (0 = unknown/unreliable).
  uint8_t battery{0};
};

// Controller lifecycle as exposed on the wire (state.ds4).
enum class ControllerState : uint8_t {
  Unpaired = 0,   // scanning, no controller ever connected
  Pairing,        // a controller is mid-association (glue-reported)
  Connected,      // ≥1 controller connected
  Disconnected,   // a previously connected controller dropped (none connected)
  Reconnecting,   // a known controller is re-associating
};
constexpr uint8_t kControllerStateCount = 5;
constexpr const char* const kControllerStateNames[kControllerStateCount] = {
    "unpaired", "pairing", "connected", "disconnected", "reconnecting",
};

const char* controller_state_name(ControllerState state);

// Event sink for mapped output (the runtime queue adapter implements it).
class EventSink {
 public:
  virtual ~EventSink() = default;
  virtual void push(uint8_t player, InputEvent event) = 0;
};

// Pure per-controller state + the shared provider view. No hardware calls,
// no timers beyond the caller's monotonic clock (single timebase contract).
class Ds4Core {
 public:
  void begin(bool enabled, uint32_t now_ms);

  // Glue-reported lifecycle. `controller_id` is the transport's handle
  // (Bluepad32 index); slots are assigned in connect order (P1 first).
  void on_connect(uint8_t controller_id, uint32_t now_ms);
  void on_disconnect(uint8_t controller_id, uint32_t now_ms);
  void on_pairing_started(uint32_t now_ms);
  void set_phone_mask(uint8_t mask) { phone_mask_ = mask & 3u; }
  void set_preferred_player(uint8_t player) { if (player == 1 || player == 2) preferred_player_ = player; }
  bool assign(uint8_t controller_id, uint8_t player);
  uint8_t player_for(uint8_t controller_id) const;
  uint8_t occupied_mask() const;
  uint8_t take_lost_mask() { const uint8_t mask = lost_mask_; lost_mask_ = 0; return mask; }

  // Feed one controller snapshot; emits edge-decoded player-tagged events
  // (d-pad with auto-repeat, face buttons on press edges). Snapshots from
  // unassigned controllers are ignored (no slot → no events).
  void apply_snapshot(uint8_t controller_id, const Snapshot& snap, uint32_t now_ms,
                      EventSink* out);

  // Wire view for GET /state.
  bool enabled() const { return enabled_; }
  ControllerState state() const { return state_; }
  uint8_t players_connected() const { return players_connected_; }
  uint8_t battery_p1() const { return battery_[0]; }
  uint8_t battery_p2() const { return battery_[1]; }

 private:
  struct Slot {
    bool used{false};
    uint8_t controller_id{0};
    Snapshot prev{};         // last snapshot (edge detection)
    bool have_prev{false};
    // d-pad auto-repeat state (one decoder per direction semantics: the
    // repeat applies to the currently-held direction).
    bool dir_down{false};
    InputEvent dir_event{InputEvent::Up};
    uint32_t dir_pressed_at_ms{0};
    uint32_t dir_next_repeat_ms{0};
    uint32_t last_seen_ms{0};
  };

  static constexpr uint32_t kRepeatDelayMs = 400;
  static constexpr uint32_t kRepeatPeriodMs = 150;

  int8_t slot_for_(uint8_t controller_id) const;
  void emit_direction_(Slot& slot, bool up, bool down, bool left, bool right, uint32_t now_ms,
                       EventSink* out);
  static bool dpad_has_(uint8_t dpad, uint8_t mask) { return (dpad & mask) != 0; }

  bool enabled_{false};
  ControllerState state_{ControllerState::Unpaired};
  uint8_t players_connected_{0};
  Slot slots_[kMaxControllers];
  uint8_t battery_[kMaxControllers]{0, 0};
  uint8_t phone_mask_{0};
  uint8_t lost_mask_{0};
  uint8_t preferred_player_{1};
};

}  // namespace ds4
}  // namespace ledwall
