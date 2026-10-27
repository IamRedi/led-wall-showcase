#pragma once

#include <cstdint>

#include "api/command.h"

namespace ledwall {

enum class SceneStepKind : uint8_t { Text = 0, Effect, Wait, GameReady };
struct SceneStep {
  SceneStepKind kind{SceneStepKind::Wait};
  char content[kTextMaxLen]{};  // text or registered animation name
  uint32_t duration_ms{1000};
};
struct SceneSpec {
  char name[25]{};
  SceneStep steps[8]{};
  uint8_t count{0};
  bool loop{false};
};

// A single device-owned foreground timeline. The browser may disconnect;
// deadlines still advance from the ESP32 monotonic clock. No dynamic storage.
class SceneTimeline {
 public:
  bool start(const SceneSpec& spec, uint32_t now_ms);
  bool tick(uint32_t now_ms);  // true when caller must apply a new step
  void stop();
  bool active() const { return active_; }
  const SceneStep* current() const { return active_ ? &spec_.steps[index_] : nullptr; }
  const char* name() const { return active_ ? spec_.name : ""; }
  uint8_t index() const { return index_; }
 private:
  SceneSpec spec_{};
  uint32_t step_started_ms_{0};
  uint8_t index_{0};
  bool active_{false};
};

bool scene_preset(const char* name, SceneSpec* out);

}  // namespace ledwall
