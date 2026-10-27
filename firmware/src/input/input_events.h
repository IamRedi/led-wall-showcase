#pragma once

#include <cstdint>

namespace ledwall {

// LED-L009 unified input model (reconstruction brief §10). Every physical
// control, API call, and future controller translates into these normalized
// events; menu/games/timer logic never sees a GPIO, a packet, or a device —
// only InputEvent (provider independence).
enum class InputEvent : uint8_t {
  Up = 0,
  Down,
  Left,
  Right,
  Confirm,      // A
  Back,         // B
  StartPause,
  Home,
  ActionB,      // gameplay B; Back remains the explicit exit/navigation key
};
constexpr uint8_t kInputEventEnumCount = 9;

const char* input_event_name(InputEvent event);        // nullptr for invalid
bool input_event_from_name(const char* name, InputEvent* out);

// LED-L010 explicit player slots (expansion brief §9). Events are attributed
// to a player: 1 = P1 (the historic single-player path), 2 = P2. Providers
// without a player concept (GPIO buttons, menu keys, the legacy API field)
// always emit P1. Games consume player-tagged events; single-user consumers
// (menu, timer, standby) ignore the attribution.
constexpr uint8_t kPlayer1 = 1;
constexpr uint8_t kPlayer2 = 2;
constexpr uint8_t kPlayerMax = 2;
enum class InputSource : uint8_t { Provider = 0, PhoneSession, Ds4 };

struct PlayerEvent {
  PlayerEvent() : player(kPlayer1), event(InputEvent::Up), source(InputSource::Provider) {}
  PlayerEvent(uint8_t p, InputEvent e, InputSource s = InputSource::Provider)
      : player(p), event(e), source(s) {}
  uint8_t player;
  InputEvent event;
  InputSource source;
};

// Bounded event queue shared by all providers (GPIO, API/phone, DS4, future
// wired controller). Overflow drops the newest event and counts it — providers
// can never grow memory, and consumers drain every tick.
class InputEventQueue {
 public:
  static constexpr uint8_t kCapacity = 16;

  void push(InputEvent event) { push(kPlayer1, event); }

  void push(uint8_t player, InputEvent event, InputSource source = InputSource::Provider) {
    if (count_ >= kCapacity) {
      ++dropped_;
      return;  // bounded: drop excess
    }
    items_[(head_ + count_) % kCapacity] = PlayerEvent{player, event, source};
    ++count_;
  }

  bool pop(InputEvent* out) {
    PlayerEvent pe{};
    if (!pop(&pe)) {
      return false;
    }
    if (out != nullptr) {
      *out = pe.event;
    }
    return true;
  }

  bool pop(PlayerEvent* out) {
    if (count_ == 0) {
      return false;
    }
    if (out != nullptr) {
      *out = items_[head_];
    }
    head_ = static_cast<uint8_t>((head_ + 1) % kCapacity);
    --count_;
    return true;
  }

  uint8_t pending() const { return count_; }
  uint32_t dropped() const { return dropped_; }
  void clear() {
    head_ = 0;
    count_ = 0;
  }

  void discard_phone(uint8_t player_mask) {
    PlayerEvent kept[kCapacity]{};
    uint8_t retained = 0;
    PlayerEvent event{};
    while (pop(&event)) {
      const uint8_t bit = event.player >= kPlayer1 && event.player <= kPlayer2
                              ? static_cast<uint8_t>(1u << (event.player - 1u)) : 0;
      if (event.source != InputSource::PhoneSession || (player_mask & bit) == 0)
        kept[retained++] = event;
    }
    for (uint8_t i = 0; i < retained; ++i)
      push(kept[i].player, kept[i].event, kept[i].source);
  }

  void discard_ds4(uint8_t player_mask) {
    PlayerEvent kept[kCapacity]{};
    uint8_t retained = 0;
    PlayerEvent event{};
    while (pop(&event)) {
      const uint8_t bit = event.player >= kPlayer1 && event.player <= kPlayer2
                              ? static_cast<uint8_t>(1u << (event.player - 1u)) : 0;
      if (event.source != InputSource::Ds4 || (player_mask & bit) == 0)
        kept[retained++] = event;
    }
    for (uint8_t i = 0; i < retained; ++i)
      push(kept[i].player, kept[i].event, kept[i].source);
  }

 private:
  PlayerEvent items_[kCapacity]{};
  uint8_t head_{0};
  uint8_t count_{0};
  uint32_t dropped_{0};
};

}  // namespace ledwall
