#include "sound/sound_scheduler.h"

namespace ledwall {
namespace {
ToneIntent tone_for(SoundCue cue) {
  switch (cue) {
    case SoundCue::Button: return {700, 35, 1};
    case SoundCue::Hit: return {880, 55, 2};
    case SoundCue::Score: return {1046, 120, 3};
    case SoundCue::GameStart: return {660, 120, 3};
    case SoundCue::Notification: return {740, 140, 2};
    case SoundCue::Launch: return {780, 180, 3};
    case SoundCue::Explosion: return {330, 190, 4};
    case SoundCue::Error: return {230, 180, 5};
    case SoundCue::Lose: return {260, 260, 5};
    case SoundCue::Win: return {1320, 260, 5};
    case SoundCue::GameOver: return {520, 220, 4};
    case SoundCue::TimerDone: return {1000, 350, 5};
  }
  return {};
}
}  // namespace

void SoundScheduler::play(SoundCue cue) {
  if (muted_) return;
  const ToneIntent tone = tone_for(cue);
  if (tone.duration_ms == 0) return;
  if (count_ >= kQueueCapacity) {
    uint8_t worst = 0;
    for (uint8_t i = 1; i < count_; ++i)
      if (queue_[i].tone.priority < queue_[worst].tone.priority) worst = i;
    if (tone.priority <= queue_[worst].tone.priority) return;
    queue_[worst] = Pending{tone, cue};
  } else queue_[count_++] = Pending{tone, cue};
}

void SoundScheduler::tick(uint32_t now_ms) {
  if (muted_) { clear(); return; }
  if (active_ && now_ms - started_ms_ < current_.duration_ms) return;
  active_ = false;
  if (count_ == 0) return;
  uint8_t best = 0;
  for (uint8_t i = 1; i < count_; ++i)
    if (queue_[i].tone.priority > queue_[best].tone.priority) best = i;
  current_ = queue_[best].tone;
  for (uint8_t i = best + 1; i < count_; ++i) queue_[i - 1] = queue_[i];
  --count_;
  started_ms_ = now_ms;
  active_ = true;
}

void SoundScheduler::clear() { count_ = 0; active_ = false; current_ = ToneIntent{}; }
void SoundScheduler::set_muted(bool muted) { muted_ = muted; if (muted) clear(); }

}  // namespace ledwall
