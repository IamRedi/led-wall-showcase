#pragma once

#include <cstdint>

#include "render/matrix_mapping.h"

namespace ledwall {

// Identity/capability handshake payload (frozen spec §10). Pure data builder;
// the transports serialize it. Conditional capabilities are advertised only if
// implemented/hardware-confirmed (§5/§10): spectrum requires confirmed mic
// hardware (omitted — unconfirmed), color and paint.diagnostic are not
// implemented in this milestone (omitted).
constexpr uint8_t kCoreCapabilityCount = 8;
constexpr const char* const kCoreCapabilities[kCoreCapabilityCount] = {
    "display.text.show",  "display.text.clear", "display.scene.set",
    "display.animation.set", "display.expression.set", "display.clear",
    "display.brightness.set", "display.state.read",
};

// LED-L009 implemented extensions (capability discovery per the
// reconstruction brief §22): always advertised because always implemented.
// `display.spectrum.*` stays conditional (mic hardware unconfirmed — §5/§14).
// LED-L010: `display.zones` (scene/animation targets + tail state view).
constexpr uint8_t kExtendedCapabilityCount = 9;
constexpr const char* const kExtendedCapabilities[kExtendedCapabilityCount] = {
    "display.timer",       "display.games",   "display.menu",
    "display.input",       "display.notify",  "display.standby",
    "display.visualizer",  "text.styles",     "display.zones",
};

// Repair cycle 1 (M-L010-4): conditional capabilities are appended only when
// their feature is compiled/confirmed, and the array is sized for the WORST
// CASE — all conditionals set (spectrum start+stop on confirmed mic hardware
// AND display.ds4 in a DS4 build). The previous +2 literal under-counted by
// one and would have written capabilities[kMaxCapabilities] out of bounds in
// exactly that build (unreachable only because spectrum was hardcoded false).
constexpr uint8_t kConditionalCapabilityCount = 3;
constexpr uint8_t kMaxCapabilities =
    kCoreCapabilityCount + kExtendedCapabilityCount + kConditionalCapabilityCount;

// Compile-time truth of the DS4 provider (esp32_eq_ds4 build only). Exposed
// as a constant so build_identity stays pure/host-testable with an explicit
// flag while production callers keep the compile-time default.
#ifdef LED_WALL_ENABLE_DS4
constexpr bool kDs4ProviderCompiled = true;
#else
inline constexpr bool kDs4ProviderCompiled = false;
#endif

struct Identity {
  char device_id[32]{};
  char device_type[24]{"visual_display_v1"};
  char role[16]{"display"};
  uint32_t protocol_version{1};
  char fw_version[24]{};
  const char* capabilities[kMaxCapabilities]{};
  uint8_t capability_count{0};
  uint8_t matrix_width{kMatrixWidth};
  uint8_t matrix_height{kMatrixHeight};
};

// Builds the identity payload. `device_id` must be a stable "led-wall-<suffix>"
// string (runtime-owned). `spectrum_available` comes from hardware confirmation
// (false until §22 mic verification). `ds4_compiled` defaults to the
// compile-time DS4 truth so the canonical build never advertises the
// controller; host tests pass it explicitly to exercise the worst case.
Identity build_identity(const char* device_id, const char* fw_version, bool spectrum_available,
                        bool ds4_compiled = kDs4ProviderCompiled);

}  // namespace ledwall
