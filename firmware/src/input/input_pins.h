#pragma once

// LED-L009 candidate GPIO pin map for the physical joystick/button shield.
//
// HARDWARE FACTS STATUS (truthful): the Owner owns an Arduino-style
// joystick/button shield, but NO repository evidence documents its pinout —
// the legacy PLUTO EQ ACCESSORY repository contains no input code (only the
// ADC microphone pin 34). The mapping below is therefore a CANDIDATE for the
// common six-control shield layout, chosen from ESP32-safe pins, and the
// provider ships DISABLED (input.enabled default 0 / no pins configured).
// It must only be enabled after the Owner confirms the actual wiring.
//
// ESP32 pin-safety notes baked into this map:
// - GPIO 13 is the LED data line — never used here.
// - GPIO 27 is the TAIL data line since LED-L010 (ADR-LED-L010 D2 GPIO audit)
//   — never used here; the former candidate `left` button moved to GPIO 4.
// - GPIO 16/17 are RESERVED for the future wired P1 RP2040 controller node
//   (UART2) — the former candidate confirm/back buttons moved to 26/14.
// - Strapping pins (0, 2, 5, 12, 15) are avoided (pull-ups at boot would
//   change boot mode; GPIO 12 specifically must stay low at boot).
// - Input-only pins 34/35 (no internal pull-ups, ADC1 — usable while WiFi is
//   active) are reserved for the analog joystick axes if the shield exposes
//   separate X/Y analog outputs.
// - All button pins below have internal pull-ups and are input-capable.

namespace ledwall {
namespace input_pins {

struct PinMap {
  int up;
  int down;
  int left;
  int right;
  int confirm;      // A
  int back;         // B
  int start_pause;
  int home;
  int axis_x;       // -1 = unused (analog, ADC1)
  int axis_y;       // -1 = unused (analog, ADC1)
  bool active_low;  // buttons wired to ground
};

// Candidate map (unverified against the physical shield — see above).
// LED-L010 reassignments (GPIO audit, ADR-LED-L010 D2): left 27→4 (27 is the
// tail data line), back 16→14 and start_pause 17→19 (16/17 reserved for the
// future wired-controller UART).
constexpr PinMap kCandidateMap = {
    32,  33,  // up, down
    4,   25,  // left, right
    26,  14,  // confirm, back
    19,  21,  // start_pause, home
    34,  35,  // axis_x, axis_y (ADC1, input-only)
    true,     // active_low
};

// Axis threshold for the ADC ladder: mid-rail deadband.
constexpr uint32_t kAxisCenter = 2048;
constexpr uint32_t kAxisDeadband = 900;

}  // namespace input_pins
}  // namespace ledwall
