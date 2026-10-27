#pragma once

// LED Wall device constants — PRESERVE DATA from legacy
// `PLUTO EQ ACCESSORY/include/eq_config.h` (frozen spec §2 VERIFIED facts).
// Audio/FFT and text-scroll constants arrive with their own phases.
//
// LED-L010 expansion (ADR-LED-L010): the tail zone constants join the SAME
// canonical hardware boundary — no render/game code may carry a GPIO literal.
// GPIO audit (ADR-LED-L010 D2, 2026-10-11): the classic-ESP32 esp32dev claims
// GPIO13 (matrix), 6-11 (flash), 1/3 (UART0), 0/2/5/12/15 (strapping, avoided),
// 34/35 (input-only ADC1: candidate joystick axes; 34 = legacy mic pin), and a
// DISABLED/unverified candidate button map (21/25/26/32/33 + 4/14/19 after the
// LED-L010 reassignment). GPIO27 was candidate `left` and is now canonical for
// the TAIL. GPIO16/17 are RESERVED for the future wired P1 RP2040 controller
// node (UART2); the P2 path is a remapped UART1 on free pins (e.g. 22/23).

#define LED_WALL_DATA_PIN 13

// LED-L010 tail zone data output — audit-locked in ADR-LED-L010 D2. Do NOT
// chain the tail onto GPIO13: the zones need independent semantic control.
#define LED_WALL_TAIL_DATA_PIN 27

#define LED_WALL_MATRIX_W 30
#define LED_WALL_MATRIX_H 10
#define LED_WALL_LED_COUNT (LED_WALL_MATRIX_W * LED_WALL_MATRIX_H)

// Tail zone bounds (LED-L010). The physical rope is ~2 m diffused addressable
// strip; its ACTUAL LED count is a hardware fact the Owner records at wiring
// time (config key `tail.length`, 1..MAX — never guessed by software). The
// default below is a clearly-documented PLACEHOLDER, not a measurement; the
// 300 cap bounds power/render cost per frame until §22 production electrical
// verification states a real budget.
#define LED_WALL_TAIL_MAX_LENGTH 300
#define LED_WALL_TAIL_LENGTH_DEFAULT 60  // PLACEHOLDER — UNVERIFIED until Owner count

// Heritage brightness evidence (frozen spec §2): EQ_BRIGHTNESS 40. Used as the
// conservative default for both semantic level and local ceiling (ADR-011)
// until §22 production power verification measures a real ceiling.
#define LED_WALL_BRIGHTNESS_DEFAULT 40
#define LED_WALL_BRIGHTNESS_CEILING_DEFAULT 40
