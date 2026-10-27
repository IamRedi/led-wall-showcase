#pragma once

#include <cstddef>
#include <cstdio>

namespace ledwall {

// Bounded operational warnings surfaced through public state (frozen spec §4
// `warnings[]`, §18). Leaf header: no repository dependencies.
constexpr uint8_t kMaxWarnings = 8;
constexpr size_t kMaxConfigWarningLen = 96;

struct Warnings {
  char items[kMaxWarnings][kMaxConfigWarningLen]{};
  uint8_t count{0};
};

inline void add_warning(Warnings* warnings, const char* text) {
  if (warnings == nullptr || text == nullptr || text[0] == '\0') {
    return;
  }
  if (warnings->count >= kMaxWarnings) {
    return;  // bounded: drop excess rather than grow
  }
  std::snprintf(warnings->items[warnings->count], kMaxConfigWarningLen, "%s", text);
  ++warnings->count;
}

}  // namespace ledwall
