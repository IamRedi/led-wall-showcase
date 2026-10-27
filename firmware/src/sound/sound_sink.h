#pragma once

#include <cstdint>

namespace ledwall {

// LED-L009 sound-cue seam (reconstruction brief §19). Semantic cues are
// emitted at the right moments (timer done, game start/over, notification,
// button) into a pluggable sink. NO speaker hardware exists in this
// repository: the default sink is a no-op, nothing is advertised in
// capabilities, and no pin is claimed. A future authorized speaker milestone
// implements a real sink behind this exact boundary.
enum class SoundCue : uint8_t {
  TimerDone = 0,
  GameStart,
  GameOver,
  Notification,
  Button,
  Hit,
  Score,
  Error,
  Win,
  Lose,
  Launch,
  Explosion,
};

class SoundSink {
 public:
  virtual ~SoundSink() = default;
  virtual void play(SoundCue cue) = 0;
};

// The only sink that exists today: semantic cues are observable in host
// tests (fake sinks) and silently dropped on the device.
class NullSoundSink : public SoundSink {
 public:
  void play(SoundCue /*cue*/) override {}
};

}  // namespace ledwall
