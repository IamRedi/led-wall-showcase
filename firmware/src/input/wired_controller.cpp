#include "input/wired_controller.h"

namespace ledwall {
namespace wired {

uint8_t crc8(const uint8_t* data, uint8_t len) {
  uint8_t crc = 0x00;
  for (uint8_t i = 0; i < len; ++i) {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8; ++bit) {
      crc = (crc & 0x80u) ? static_cast<uint8_t>((crc << 1) ^ 0x07u)
                          : static_cast<uint8_t>(crc << 1);
    }
  }
  return crc;
}

uint8_t encode(const Packet& packet, uint8_t* frame) {
  if (frame == nullptr) {
    return 0;
  }
  frame[0] = kMagic0;
  frame[1] = kMagic1;
  frame[2] = (packet.player == 2u) ? 2u : 1u;
  frame[3] = static_cast<uint8_t>(packet.dpad & 0x0Fu);
  frame[4] = static_cast<uint8_t>(packet.keys & 0x0Fu);
  frame[5] = packet.axis_x;
  frame[6] = packet.axis_y;
  frame[7] = packet.joystick_push ? 1u : 0u;
  frame[8] = packet.seq;
  frame[9] = crc8(&frame[2], 7);
  return kFrameLen;
}

DecodeResult decode(const uint8_t* frame, uint8_t len) {
  DecodeResult r;
  if (frame == nullptr || len < kFrameLen) {
    return r;
  }
  if (frame[0] != kMagic0 || frame[1] != kMagic1) {
    return r;
  }
  if (frame[9] != crc8(&frame[2], 7)) {
    return r;
  }
  r.ok = true;
  r.packet.player = (frame[2] == 2u) ? 2u : 1u;
  r.packet.dpad = static_cast<uint8_t>(frame[3] & 0x0Fu);
  r.packet.keys = static_cast<uint8_t>(frame[4] & 0x0Fu);
  r.packet.axis_x = frame[5];
  r.packet.axis_y = frame[6];
  r.packet.joystick_push = frame[7] != 0;
  r.packet.seq = frame[8];
  return r;
}

}  // namespace wired
}  // namespace ledwall
