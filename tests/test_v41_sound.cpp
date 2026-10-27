#include "test_framework.h"

#include "sound/sound_scheduler.h"

using namespace ledwall;

void test_v41_sound_scheduler_priority_mute_and_bounds() {
  SoundScheduler sound;
  sound.play(SoundCue::Button);
  sound.play(SoundCue::TimerDone);
  sound.play(SoundCue::GameStart);
  sound.tick(1000);
  CHECK(sound.active());
  CHECK_EQ_INT(sound.current().hz, 1000);
  CHECK_EQ_INT(sound.queued(), 2);
  sound.tick(1349);
  CHECK_EQ_INT(sound.current().hz, 1000);
  sound.tick(1350);
  CHECK_EQ_INT(sound.current().hz, 660);
  for (int i = 0; i < 20; ++i) sound.play(SoundCue::Button);
  CHECK(sound.queued() <= SoundScheduler::kQueueCapacity);
  sound.set_muted(true);
  CHECK(!sound.active());
  CHECK_EQ_INT(sound.queued(), 0);
  sound.play(SoundCue::Win);
  sound.tick(2000);
  CHECK(!sound.active());
  sound.set_muted(false);
  sound.play(SoundCue::Explosion);
  sound.tick(2001);
  CHECK_EQ_INT(sound.current().hz, 330);
  sound.clear();
  CHECK(!sound.active());
}
