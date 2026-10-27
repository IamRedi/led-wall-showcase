#include "scene/scene_timeline.h"

#include <cstring>

#include "api/registry.h"

namespace ledwall {

bool SceneTimeline::start(const SceneSpec& spec, uint32_t now_ms) {
  if (spec.count == 0 || spec.count > 8 || spec.name[0] == '\0') return false;
  for (uint8_t i = 0; i < spec.count; ++i) {
    const SceneStep& step = spec.steps[i];
    if (step.duration_ms < 100 || step.duration_ms > 600000) return false;
    if (step.kind == SceneStepKind::Text && step.content[0] == '\0') return false;
    if (step.kind == SceneStepKind::Effect && !valid_animation(step.content)) return false;
    if (step.kind == SceneStepKind::GameReady && !valid_game(step.content)) return false;
    if (step.kind != SceneStepKind::Text && step.kind != SceneStepKind::Effect &&
        step.kind != SceneStepKind::Wait && step.kind != SceneStepKind::GameReady) return false;
  }
  spec_ = spec;
  index_ = 0;
  step_started_ms_ = now_ms;
  active_ = true;
  return true;
}

bool SceneTimeline::tick(uint32_t now_ms) {
  if (!active_) return false;
  bool changed = false;
  uint8_t budget = 8;
  while (active_ && budget-- &&
         now_ms - step_started_ms_ >= spec_.steps[index_].duration_ms) {
    step_started_ms_ += spec_.steps[index_].duration_ms;
    ++index_;
    if (index_ >= spec_.count) {
      if (spec_.loop) index_ = 0;
      else { active_ = false; break; }
    }
    changed = true;
  }
  if (active_ && now_ms - step_started_ms_ >= spec_.steps[index_].duration_ms) {
    // A long stall cannot cause an unbounded catch-up loop.
    step_started_ms_ = now_ms;
  }
  return changed;
}

void SceneTimeline::stop() { active_ = false; }

bool scene_preset(const char* name, SceneSpec* out) {
  if (!name || !out) return false;
  SceneSpec spec;
  std::strncpy(spec.name, name, sizeof(spec.name) - 1);
  spec.count = 3;
  spec.steps[0].kind = SceneStepKind::Text;
  spec.steps[0].duration_ms = 3000;
  spec.steps[1].kind = SceneStepKind::Effect;
  spec.steps[1].duration_ms = 4800;
  spec.steps[2].kind = SceneStepKind::Effect;
  spec.steps[2].duration_ms = 4800;
  if (std::strcmp(name, "calm") == 0) {
    std::strcpy(spec.steps[0].content, "RELAX");
    std::strcpy(spec.steps[1].content, "aurora_current");
    spec.count = 2;
    spec.loop = true;
  } else if (std::strcmp(name, "welcome") == 0) {
    std::strcpy(spec.steps[0].content, "WELCOME");
    std::strcpy(spec.steps[1].content, "energy_bloom");
    std::strcpy(spec.steps[2].content, "fireworks");
  } else if (std::strcmp(name, "birthday") == 0) {
    std::strcpy(spec.steps[0].content, "HAPPY BIRTHDAY");
    std::strcpy(spec.steps[1].content, "rainbow_river");
    std::strcpy(spec.steps[2].content, "fireworks");
  } else if (std::strcmp(name, "celebration") == 0) {
    std::strcpy(spec.steps[0].content, "CELEBRATE");
    std::strcpy(spec.steps[1].content, "starstream");
    std::strcpy(spec.steps[2].content, "fireworks");
  } else if (std::strcmp(name, "game_night") == 0) {
    std::strcpy(spec.steps[0].content, "GAME NIGHT");
    std::strcpy(spec.steps[1].content, "energy_bloom");
    spec.steps[2].kind = SceneStepKind::GameReady;
    spec.steps[2].duration_ms = 100;
    std::strcpy(spec.steps[2].content, "pong");
  } else if (std::strcmp(name, "countdown_finale") == 0) {
    spec.count = 4;
    std::strcpy(spec.steps[0].content, "3");
    spec.steps[0].duration_ms = 1000;
    spec.steps[1].kind = SceneStepKind::Text;
    std::strcpy(spec.steps[1].content, "2");
    spec.steps[1].duration_ms = 1000;
    spec.steps[2].kind = SceneStepKind::Text;
    std::strcpy(spec.steps[2].content, "1");
    spec.steps[2].duration_ms = 1000;
    spec.steps[3].kind = SceneStepKind::Effect;
    std::strcpy(spec.steps[3].content, "fireworks");
    spec.steps[3].duration_ms = 4800;
  } else return false;
  *out = spec;
  return true;
}

}  // namespace ledwall
