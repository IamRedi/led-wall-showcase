#pragma once

#include <cstdint>

// LED-L010 wired-controller protocol seam (ADR-LED-L010 D6; expansion brief
// §10). The Owner's future controllers are two Funduino-style joystick/button
// shields, each read by one RP2040-Zero node and connected to the LED Wall
// ESP32 over a 4-wire coiled cable (power, ground, TX, RX — a UART). This
// module reserves the seam NOW: a pure packet codec, host-tested, so the
// future node firmware and the ESP32 side share one exact wire format. The
// UART wiring (P1 on UART2 GPIO16/17; P2 on a remapped UART1, e.g. 22/23) is
// documented in ADR-LED-L010 D2 — NO UART is claimed by this module and no
// RP2040 node firmware exists in this milestone.
//
// Frame (10 bytes, little-endian fields, 115200 8N1 recommended):
//   [0] 0xAA  magic 1
//   [1] 0x55  magic 2
//   [2] player        1 | 2 (wired slot; node-strapped)
//   [3] dpad_bits     bit0 up, bit1 down, bit2 left, bit3 right (1 = pressed)
//   [4] key_bits      bit0 confirm(A), bit1 back(B), bit2 start_pause,
//                     bit3 home (1 = pressed)
//   [5] axis_x        0..255, 128 = center
//   [6] axis_y        0..255, 128 = center
//   [7] joystick_push bit0 = stick click
//   [8] seq           rolling counter (any byte; freshness only)
//   [9] crc8          CRC-8 (poly 0x07, init 0x00) over bytes [2..8]
//
// Nodes emit frames on change + a 100 ms heartbeat. The ESP32-side decoder
// feeds the unified input queue as player-attributed events (same stream as
// DS4 and the phone). Axis handling mirrors the GPIO provider's deadband.

namespace ledwall {
namespace wired {

constexpr uint8_t kFrameLen = 10;
constexpr uint8_t kMagic0 = 0xAA;
constexpr uint8_t kMagic1 = 0x55;

// dpad_bits masks.
namespace dpad_bits {
constexpr uint8_t kUp = 0x1;
constexpr uint8_t kDown = 0x2;
constexpr uint8_t kLeft = 0x4;
constexpr uint8_t kRight = 0x8;
}  // namespace dpad_bits

// key_bits masks.
namespace key_bits {
constexpr uint8_t kConfirm = 0x1;
constexpr uint8_t kBack = 0x2;
constexpr uint8_t kStartPause = 0x4;
constexpr uint8_t kHome = 0x8;
}  // namespace key_bits

struct Packet {
  uint8_t player{1};  // 1 | 2
  uint8_t dpad{0};    // dpad_bits mask
  uint8_t keys{0};    // key_bits mask
  uint8_t axis_x{128};
  uint8_t axis_y{128};
  bool joystick_push{false};
  uint8_t seq{0};
};

uint8_t crc8(const uint8_t* data, uint8_t len);  // poly 0x07, init 0x00

// Encodes a packet into frame[10]; returns kFrameLen.
uint8_t encode(const Packet& packet, uint8_t* frame);

struct DecodeResult {
  bool ok{false};       // true iff a complete, CRC-valid frame was decoded
  Packet packet{};
};

// Strict single-frame decoder: decodes EXACTLY frame[0..9]. Byte-stream
// resynchronization (multiple/shifted frames in one buffer) is the UART
// reader's job in the future milestone — this seam stays minimal and total.
DecodeResult decode(const uint8_t* frame, uint8_t len);

}  // namespace wired
}  // namespace ledwall
