#pragma once

#include <cstdint>

namespace ledwall {
namespace effects {

// Frozen §8 expression vocabulary → visual realization mapping (ADR-007: the
// mappings are internal data, changeable without API change; frozen spec §8:
// "the mapping table lives as versioned data (config/registry), not buried in
// code paths"). No independent expression state machine exists: an expression
// is a data row selecting an existing canonical renderer with parameters.
//
// Frozen spec examples implemented exactly: thinking → slow wave; alert →
// border with an alert (red) palette; sleeping → dim idle (rest).
// idle/rest renders the safe blank frame (the §11 idle look, "dim" realized
// through the user brightness pipeline).
struct ExpressionVisual {
  const char* expression;  // frozen §8 name (registry-identical)
  enum class Kind : uint8_t {
    Rest,         // blank/rest frame (idle, sleeping)
    BorderPulse,  // pulsing border; hue = palette, period from speed param
    StaticBorder, // non-animated solid border (error)
    Wave,         // travelling sine; speed divisor + hue offset
    Plasma,       // additive-sine field; time divisor
  };
  Kind kind;
  uint32_t param_a;  // BorderPulse: period ms | Wave: time divisor | Plasma: time divisor
  uint8_t hue;       // palette hue (alert = 0 red; listening = 160 heritage border hue)
  uint8_t hue_offset;  // Wave per-column hue offset (0 = heritage sweep, +48 warm)
  bool animated;     // does the visual change over time (dirty-marking)?
};

// The 8 frozen intents in registry order. Lookup by name; unknown names are
// rejected upstream by the dispatcher (unknownExpression) and render blank
// here (fail closed).
const ExpressionVisual* expression_visual_by_name(const char* name);

}  // namespace effects
}  // namespace ledwall
