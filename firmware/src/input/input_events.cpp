#include "input/input_events.h"

#include <cstring>

namespace ledwall {

namespace {
// Wire names in enum order — mirrors registry kInputEventNames (pinned by test).
const char* const kWireNames[kInputEventEnumCount] = {
    "up", "down", "left", "right", "confirm", "back", "start_pause", "home", "action_b",
};
}  // namespace

const char* input_event_name(InputEvent event) {
  if (static_cast<uint8_t>(event) < kInputEventEnumCount) {
    return kWireNames[static_cast<uint8_t>(event)];
  }
  return nullptr;
}

bool input_event_from_name(const char* name, InputEvent* out) {
  if (name == nullptr) {
    return false;
  }
  for (uint8_t i = 0; i < kInputEventEnumCount; ++i) {
    if (std::strcmp(name, kWireNames[i]) == 0) {
      if (out != nullptr) {
        *out = static_cast<InputEvent>(i);
      }
      return true;
    }
  }
  return false;
}

}  // namespace ledwall
