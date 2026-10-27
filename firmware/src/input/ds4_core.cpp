#include "input/ds4_core.h"

#include <cstring>

namespace ledwall {
namespace ds4 {

const char* controller_state_name(ControllerState state) {
  if (static_cast<uint8_t>(state) < kControllerStateCount) {
    return kControllerStateNames[static_cast<uint8_t>(state)];
  }
  return kControllerStateNames[0];
}

void Ds4Core::begin(bool enabled, uint32_t now_ms) {
  enabled_ = enabled;
  state_ = ControllerState::Unpaired;
  players_connected_ = 0;
  for (Slot& s : slots_) {
    s = Slot{};  // value-reset (Slot is non-trivial)
  }
  battery_[0] = 0;
  battery_[1] = 0;
  phone_mask_ = 0;
  lost_mask_ = 0;
  preferred_player_ = 1;
  (void)now_ms;
}

int8_t Ds4Core::slot_for_(uint8_t controller_id) const {
  for (uint8_t i = 0; i < kMaxControllers; ++i) {
    if (slots_[i].used && slots_[i].controller_id == controller_id) {
      return static_cast<int8_t>(i);
    }
  }
  return -1;
}

void Ds4Core::on_pairing_started(uint32_t now_ms) {
  (void)now_ms;
  if (!enabled_) {
    return;
  }
  if (state_ == ControllerState::Unpaired || state_ == ControllerState::Disconnected) {
    state_ = ControllerState::Pairing;
  }
}

void Ds4Core::on_connect(uint8_t controller_id, uint32_t now_ms) {
  if (!enabled_) {
    return;
  }
  int8_t existing = slot_for_(controller_id);
  if (existing >= 0) {
    slots_[existing].last_seen_ms = now_ms;
    return;  // Bluepad callbacks/retries must never increment the count twice.
  }
  if (existing < 0) {
    // Prefer P1 for the first DS4, but never take a phone-owned slot.
    const uint8_t first = static_cast<uint8_t>(preferred_player_ - 1u);
    const uint8_t second = static_cast<uint8_t>(1u - first);
    for (uint8_t order = 0; order < kMaxControllers; ++order) {
      const uint8_t i = order == 0 ? first : second;
      if (!slots_[i].used && (phone_mask_ & (1u << i)) == 0) {
        slots_[i] = Slot{};
        slots_[i].used = true;
        slots_[i].controller_id = controller_id;
        existing = static_cast<int8_t>(i);
        break;
      }
    }
  }
  if (existing >= 0) {
    slots_[existing].last_seen_ms = now_ms;
    ++players_connected_;  // capped implicitly: only two slots exist
    if (players_connected_ > kMaxControllers) {
      players_connected_ = kMaxControllers;
    }
    state_ = ControllerState::Connected;
  }
}

uint8_t Ds4Core::player_for(uint8_t controller_id) const {
  const int8_t slot = slot_for_(controller_id);
  return slot < 0 ? 0 : static_cast<uint8_t>(slot + 1);
}

uint8_t Ds4Core::occupied_mask() const {
  return static_cast<uint8_t>((slots_[0].used ? 1u : 0u) |
                              (slots_[1].used ? 2u : 0u));
}

bool Ds4Core::assign(uint8_t controller_id, uint8_t player) {
  if (player < 1 || player > 2 || (phone_mask_ & (1u << (player - 1u))) != 0)
    return false;
  const int8_t from = slot_for_(controller_id);
  if (from < 0) return false;
  const uint8_t to = static_cast<uint8_t>(player - 1u);
  if (slots_[to].used && to != static_cast<uint8_t>(from)) return false;
  if (to == static_cast<uint8_t>(from)) return true;
  lost_mask_ |= static_cast<uint8_t>(1u << from);
  slots_[to] = Slot{};
  slots_[to].used = true;
  slots_[to].controller_id = controller_id;
  slots_[from] = Slot{};
  battery_[to] = battery_[from];
  battery_[from] = 0;
  preferred_player_ = player;
  return true;
}

void Ds4Core::on_disconnect(uint8_t controller_id, uint32_t now_ms) {
  (void)now_ms;
  if (!enabled_) {
    return;
  }
  const int8_t slot = slot_for_(controller_id);
  if (slot < 0) {
    return;
  }
  slots_[slot].used = false;
  slots_[slot].have_prev = false;
  slots_[slot].dir_down = false;
  battery_[slot] = 0;
  lost_mask_ |= static_cast<uint8_t>(1u << slot);
  players_connected_ = (players_connected_ > 0) ? static_cast<uint8_t>(players_connected_ - 1) : 0;
  // A dropped controller the glue expects to re-associate reports
  // Reconnecting; a plain drop (no rejoin expected) reports Disconnected —
  // the glue calls on_pairing_started for rejoin attempts.
  state_ = (players_connected_ > 0) ? ControllerState::Connected : ControllerState::Disconnected;
}

void Ds4Core::emit_direction_(Slot& slot, bool up, bool down, bool left, bool right,
                              uint32_t now_ms, EventSink* out) {
  // Direction resolution: at most one semantic direction per hat reading
  // (diagonals resolve to the dominant axis, N/S first — deterministic).
  InputEvent want = slot.dir_event;
  bool has = slot.dir_down;
  if (up) {
    want = InputEvent::Up;
    has = true;
  } else if (down) {
    want = InputEvent::Down;
    has = true;
  } else if (left) {
    want = InputEvent::Left;
    has = true;
  } else if (right) {
    want = InputEvent::Right;
    has = true;
  }

  if (!has) {
    slot.dir_down = false;
    return;
  }
  if (!slot.dir_down || want != slot.dir_event) {
    // New press or direction change: emit once, arm repeat.
    slot.dir_down = true;
    slot.dir_event = want;
    slot.dir_pressed_at_ms = now_ms;
    slot.dir_next_repeat_ms = now_ms + kRepeatDelayMs;
    out->push(0, want);  // player filled by caller wrapper below
    return;
  }
  // Held: auto-repeat after the delay, then every period.
  if (static_cast<int32_t>(now_ms - slot.dir_next_repeat_ms) >= 0) {
    slot.dir_next_repeat_ms = now_ms + kRepeatPeriodMs;
    out->push(0, want);
  }
}

void Ds4Core::apply_snapshot(uint8_t controller_id, const Snapshot& snap, uint32_t now_ms,
                             EventSink* out) {
  if (!enabled_ || out == nullptr) {
    return;
  }
  const int8_t slot_idx = slot_for_(controller_id);
  if (slot_idx < 0) {
    return;  // unassigned controller: no slot, no events
  }
  const uint8_t player = static_cast<uint8_t>(slot_idx + 1u);  // slot 0 → P1
  Slot& slot = slots_[slot_idx];
  slot.last_seen_ms = now_ms;
  battery_[slot_idx] = snap.battery;

  // Bluepad d-pad is a bitmask, not an eight-position hat. The left stick
  // takes over only when the d-pad is released; hysteresis avoids jitter.
  bool up = dpad_has_(snap.dpad, 1);
  bool down = dpad_has_(snap.dpad, 2);
  bool right = dpad_has_(snap.dpad, 4);
  bool left = dpad_has_(snap.dpad, 8);
  if (!(up || down || left || right)) {
    const int16_t x = snap.axis_x, y = snap.axis_y;
    const int16_t ax = x < 0 ? -x : x, ay = y < 0 ? -y : y;
    const int16_t threshold = slot.dir_down ? 110 : 160;
    if (ax >= threshold || ay >= threshold) {
      if (ay >= ax) { up = y < 0; down = y > 0; }
      else { left = x < 0; right = x > 0; }
    }
  }
  if (up || down || left || right) {
    struct WrappedSink final : EventSink {
      WrappedSink(EventSink* inner_, uint8_t player_) : inner(inner_), player(player_) {}
      void push(uint8_t /*p*/, InputEvent event) override { inner->push(player, event); }
      EventSink* inner;
      uint8_t player;
    } wrapped(out, player);
    emit_direction_(slot, up, down, left, right, now_ms, &wrapped);
  } else {
    slot.dir_down = false;
  }

  // Face buttons + system keys: edge-decoded presses only.
  const Snapshot& prev = slot.prev;
  const bool have_prev = slot.have_prev;
  const auto edge = [have_prev](bool now_p, bool prev_p) {
    return now_p && (!have_prev || !prev_p);
  };
  if (edge(snap.cross, prev.cross)) {
    out->push(player, InputEvent::Confirm);
  }
  if (edge(snap.circle, prev.circle)) {
    out->push(player, InputEvent::Back);
  }
  if (edge(snap.square, prev.square)) {
    out->push(player, InputEvent::Left);
  }
  if (edge(snap.triangle, prev.triangle)) {
    out->push(player, InputEvent::Right);
  }
  if (edge(snap.options, prev.options)) {
    out->push(player, InputEvent::StartPause);
  }
  if (edge(snap.share, prev.share)) {
    out->push(player, InputEvent::Home);
  }
  if (edge(snap.l1, prev.l1)) out->push(player, InputEvent::Left);
  if (edge(snap.r1, prev.r1)) out->push(player, InputEvent::Right);

  slot.prev = snap;
  slot.have_prev = true;
}

}  // namespace ds4
}  // namespace ledwall
