#pragma once

#include <cstdint>

#include "input/ds4_core.h"
#include "input/input_events.h"

// LED-L010 DualShock 4 device glue (ADR-LED-L010 D5). Compiled ONLY in the
// esp32_eq_ds4 environment (LED_WALL_ENABLE_DS4) where the Bluepad32 Arduino
// core fork supplies the BT-Classic HID host stack. The canonical esp32_eq
// build never sees this file — no Bluetooth code, no capability claim.
//
// The glue owns the Bluepad32 callbacks and translates them once into the
// pure core (ds4_core.h): Sony vocabulary never leaves this boundary.

namespace ledwall {
namespace ds4 {

struct StateView {
  bool enabled{false};
  bool ready{false};
  ControllerState state{ControllerState::Unpaired};
  uint8_t players{0};
  uint8_t assigned_mask{0};
  uint32_t input_events{0};
  uint8_t battery_p1{0};
  uint8_t battery_p2{0};
};

// Wire name for a ControllerState (state.ds4.state).
const char* state_name(ControllerState state);

// Attach the provider: enables Bluepad32 scanning when `enabled`. Events flow
// into the shared unified input queue (player-attributed).
void begin(bool enabled, InputEventQueue* queue);

// Poll: Bluepad2 callbacks arrive on the Arduino loop; this drains any
// pending mapped events into the queue and refreshes the lifecycle view.
void tick(uint32_t now_ms, bool wifi_ready, uint8_t phone_mask);
bool pair();
bool assign_primary(uint8_t player);
bool player_claimed(uint8_t player);
uint8_t take_lost_mask();

// Current view for GET /state (supported: true in this build).
StateView view();

}  // namespace ds4
}  // namespace ledwall
