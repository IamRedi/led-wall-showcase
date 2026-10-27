#include "render/display_topology.h"

#include <cstring>

namespace ledwall {

const char* zone_target_name(ZoneTarget target) {
  if (static_cast<uint8_t>(target) < kZoneTargetCount) {
    return kZoneTargetNames[static_cast<uint8_t>(target)];
  }
  return nullptr;
}

bool zone_target_from_name(const char* name, ZoneTarget* out) {
  if (name == nullptr || out == nullptr) {
    return false;
  }
  for (uint8_t i = 0; i < kZoneTargetCount; ++i) {
    if (std::strcmp(kZoneTargetNames[i], name) == 0) {
      *out = static_cast<ZoneTarget>(i);
      return true;
    }
  }
  return false;
}

bool zone_target_supported(uint8_t supported_mask, ZoneTarget requested) {
  switch (requested) {
    case ZoneTarget::Matrix:
      return (supported_mask & zone_flags::kMatrix) != 0;
    case ZoneTarget::Tail:
      return (supported_mask & zone_flags::kTail) != 0;
    case ZoneTarget::Both:
      return (supported_mask & zone_flags::kBoth) != 0;
  }
  return false;
}

}  // namespace ledwall
