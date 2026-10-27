#include "input/input_sessions.h"

namespace ledwall {

InputSessions::Result InputSessions::claim(uint8_t player, uint32_t token, uint32_t now_ms) {
  if (player < 1 || player > 2 || token == 0) return Result::NotOwned;
  expire(now_ms);
  Slot& slot = slots_[player - 1];
  if (slot.token != 0) return Result::Busy;
  slot = Slot{};
  slot.token = token;
  slot.last_ms = now_ms;
  return Result::Accepted;
}

InputSessions::Result InputSessions::release(uint8_t player, uint32_t token) {
  if (player < 1 || player > 2 || token == 0) return Result::NotOwned;
  Slot& slot = slots_[player - 1];
  if (slot.token != token) return Result::NotOwned;
  slot = Slot{};
  lost_mask_ |= static_cast<uint8_t>(1u << (player - 1u));
  return Result::Accepted;
}

InputSessions::Result InputSessions::event(uint8_t player, uint32_t token,
                                           uint32_t sequence, InputEvent input,
                                           bool pressed, uint32_t now_ms,
                                           InputEventQueue* queue) {
  if (player < 1 || player > 2 || token == 0 || queue == nullptr ||
      static_cast<uint8_t>(input) >= kInputEventEnumCount) return Result::NotOwned;
  expire(now_ms);
  Slot& slot = slots_[player - 1];
  if (slot.token != token) return Result::NotOwned;
  if (sequence == 0 || sequence <= slot.sequence) return Result::StaleSequence;
  const uint16_t bit = static_cast<uint16_t>(1u << static_cast<uint8_t>(input));
  if (pressed && !(slot.pressed & bit) && queue->pending() >= InputEventQueue::kCapacity)
    return Result::QueueFull;
  slot.sequence = sequence;
  slot.last_ms = now_ms;
  if (pressed) {
    if (slot.pressed & bit) return Result::Duplicate;
    slot.pressed |= bit;
    queue->push(player, input, InputSource::PhoneSession);
  } else {
    slot.pressed = static_cast<uint16_t>(slot.pressed & ~bit);
  }
  return Result::Accepted;
}

InputSessions::Result InputSessions::heartbeat(uint8_t player, uint32_t token,
                                                uint32_t sequence, uint32_t now_ms) {
  if (player < 1 || player > 2 || token == 0) return Result::NotOwned;
  expire(now_ms);
  Slot& slot = slots_[player - 1];
  if (slot.token != token) return Result::NotOwned;
  if (sequence == 0 || sequence <= slot.sequence) return Result::StaleSequence;
  slot.sequence = sequence;
  slot.last_ms = now_ms;
  return Result::Accepted;
}

void InputSessions::expire(uint32_t now_ms) {
  for (uint8_t i = 0; i < 2; ++i) {
    Slot& slot = slots_[i];
    if (slot.token != 0 && now_ms - slot.last_ms >= kIdleTimeoutMs) {
      slot = Slot{};
      lost_mask_ |= static_cast<uint8_t>(1u << i);
    }
  }
}

bool InputSessions::claimed(uint8_t player) const {
  return player >= 1 && player <= 2 && slots_[player - 1].token != 0;
}

}  // namespace ledwall
