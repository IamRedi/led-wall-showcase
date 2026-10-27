#include "effects/expression_map.h"

#include <cstring>

namespace ledwall {
namespace effects {

namespace {

// Data-driven expression mapping (frozen §8 vocabulary first in registry
// order, then the LED-L009 additions; ADR-007: the mappings are internal
// data, changeable without API change; frozen spec §8: "the mapping table
// lives as versioned data (config/registry), not buried in code paths"). No
// independent expression state machine exists: an expression is a data row
// selecting an existing canonical renderer with parameters.
//
// Frozen spec examples implemented exactly: thinking → slow wave; alert →
// border with an alert (red) palette; sleeping → dim idle (rest).
// idle/rest renders the safe blank frame (the §11 idle look, "dim" realized
// through the user brightness pipeline).
// LED-L009 additions (reconstruction brief §16 HUGO expression library):
// bored → very slow dim plasma; tired → very slow cool wave; focused → calm
// cyan border pulse; excited → fast wide-spectrum wave.
constexpr ExpressionVisual kExpressionVisuals[] = {
    {"idle", ExpressionVisual::Kind::Rest, 0, 0, 0, false},
    {"listening", ExpressionVisual::Kind::BorderPulse, 5000, 160, 0, true},
    {"thinking", ExpressionVisual::Kind::Wave, 160, 0, 0, true},   // slow wave (§8)
    {"speaking", ExpressionVisual::Kind::Wave, 40, 0, 48, true},   // fast warm wave
    {"happy", ExpressionVisual::Kind::Plasma, 50, 0, 0, true},     // slow bright field
    {"alert", ExpressionVisual::Kind::BorderPulse, 1250, 0, 0, true},  // red alert pulse (§8)
    {"error", ExpressionVisual::Kind::StaticBorder, 0, 0, 0, false},   // solid red border
    {"sleeping", ExpressionVisual::Kind::Rest, 0, 0, 0, false},   // dim idle (§8)
    {"bored", ExpressionVisual::Kind::Plasma, 90, 0, 0, true},     // very slow dim field
    {"tired", ExpressionVisual::Kind::Wave, 200, 0, 0, true},      // very slow cool wave
    {"focused", ExpressionVisual::Kind::BorderPulse, 6000, 110, 0, true},  // calm cyan pulse
    {"excited", ExpressionVisual::Kind::Wave, 28, 0, 96, true},    // fast wide-spectrum wave
};

}  // namespace

const ExpressionVisual* expression_visual_by_name(const char* name) {
  if (name == nullptr || name[0] == '\0') {
    return nullptr;  // fail closed: caller renders blank
  }
  for (const ExpressionVisual& v : kExpressionVisuals) {
    if (std::strcmp(v.expression, name) == 0) {
      return &v;
    }
  }
  return nullptr;
}

}  // namespace effects
}  // namespace ledwall
