#include "test_framework.h"

#include <cstring>

#include "scene/scene_timeline.h"

using namespace ledwall;

void test_v41_scene_bounded_timeline_and_presets() {
  const char* names[] = {"calm", "welcome", "birthday", "celebration",
                         "game_night", "countdown_finale"};
  for (const char* name : names) {
    SceneSpec preset;
    CHECK(scene_preset(name, &preset));
    CHECK(preset.count == (std::strcmp(name, "countdown_finale") == 0 ? 4 :
                           std::strcmp(name, "calm") == 0 ? 2 : 3));
    if (std::strcmp(name, "game_night") == 0) {
      CHECK(preset.steps[2].kind == SceneStepKind::GameReady);
      CHECK_STR_EQ(preset.steps[2].content, "pong");
    }
    if (std::strcmp(name, "countdown_finale") == 0) {
      CHECK_STR_EQ(preset.steps[0].content, "3");
      CHECK_STR_EQ(preset.steps[1].content, "2");
      CHECK_STR_EQ(preset.steps[2].content, "1");
      CHECK_STR_EQ(preset.steps[3].content, "fireworks");
      continue;
    }
    SceneTimeline timeline;
    CHECK(timeline.start(preset, 1000));
    CHECK(timeline.current() != nullptr);
    CHECK(!timeline.tick(3999));
    CHECK(timeline.tick(4000));
    CHECK(timeline.index() == 1);
    CHECK(timeline.tick(8800));
    if (std::strcmp(name, "calm") == 0) {
      CHECK(timeline.index() == 0);
      CHECK(timeline.active());
      continue;
    }
    CHECK(timeline.index() == 2);
    CHECK(!timeline.tick(10000));
    CHECK(!timeline.tick(13600));
    CHECK(!timeline.active());
  }
  SceneSpec invalid;
  std::strcpy(invalid.name, "bad");
  invalid.count = 1;
  invalid.steps[0].kind = SceneStepKind::Effect;
  std::strcpy(invalid.steps[0].content, "does_not_exist");
  SceneTimeline timeline;
  CHECK(!timeline.start(invalid, 0));
  invalid.steps[0].kind = SceneStepKind::Wait;
  invalid.steps[0].duration_ms = 99;
  CHECK(!timeline.start(invalid, 0));
  invalid.steps[0].duration_ms = 100;
  CHECK(timeline.start(invalid, 0));
  CHECK(timeline.active());
  timeline.stop();
  CHECK(!timeline.active());
}
