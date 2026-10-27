#pragma once

#include <cstdint>

#include "sound/sound_sink.h"

namespace ledwall {

struct ToneIntent {
  constexpr ToneIntent(uint16_t frequency = 0, uint16_t duration = 0, uint8_t rank = 0)
      : hz(frequency), duration_ms(duration), priority(rank) {}
  uint16_t hz, duration_ms;
  uint8_t priority;
};

// Bounded nonblocking monophonic cue scheduler. It produces tone intentions,
// not GPIO output. The physical adapter remains absent until buzzer wiring,
// current and pinout are independently verified.
class SoundScheduler final : public SoundSink {
 public:
  static constexpr uint8_t kQueueCapacity = 4;
  void play(SoundCue cue) override;
  void tick(uint32_t now_ms);
  void clear();
  void set_muted(bool muted);
  bool muted() const { return muted_; }
  bool active() const { return active_; }
  ToneIntent current() const { return active_ ? current_ : ToneIntent{}; }
  uint8_t queued() const { return count_; }
 private:
  struct Pending { ToneIntent tone; SoundCue cue; };
  Pending queue_[kQueueCapacity]{};
  ToneIntent current_{};
  uint32_t started_ms_{0};
  uint8_t count_{0};
  bool active_{false}, muted_{false};
};

}  // namespace ledwall
