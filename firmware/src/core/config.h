#pragma once

#include <cstddef>
#include <cstdint>

#include "core/brightness.h"
#include "core/mode_machine.h"
#include "core/warnings.h"
#include "render/display_topology.h"
#include "render/matrix_mapping.h"

namespace ledwall {

// Runtime configuration (frozen spec §18). Persistence is device-side
// (NVS/Preferences, see config_store); this pure module defines the schema,
// defaults, and the fallback rules: unknown keys are ignored with a warning,
// invalid values fall back to the default without mutation.
// wifi.password is a stored secret: it is loadable/savable but is NEVER
// reported through state, identity, or events.
struct AppConfig {
  OrientationFlags orientation{kHeritageOrientation};
  uint32_t brightness_default_level{LED_WALL_BRIGHTNESS_DEFAULT};    // heritage §2
  uint32_t brightness_ceiling{LED_WALL_BRIGHTNESS_CEILING_DEFAULT};  // ADR-011 conservative
  ModeKind boot_base_mode{ModeKind::Idle};                           // safe boot default
  char wifi_ssid[33]{};                                              // §18: WiFi credentials
  char wifi_password[65]{};                                          // secret (§18)
  uint32_t http_port{80};                                            // §9 semantic HTTP API
  uint32_t ws_port{81};                                              // §9 WebSocket events
  bool input_enabled{false};  // LED-L009: physical GPIO inputs (off until wiring confirmed)
  // LED-L010 tail zone (dotted keys tail.enabled / tail.length / tail.reverse).
  // Ships disabled; length is the documented PLACEHOLDER until the Owner
  // counts the physical rope (ADR-LED-L010 D1).
  TailZoneConfig tail{};
  // LED-L010 DS4 controller provider (config key ds4.enabled). Only consumed
  // by the esp32_eq_ds4 build (LED_WALL_ENABLE_DS4); harmlessly ignored
  // elsewhere. Defaults ON in that build so pairing works out of the box.
  bool ds4_enabled{true};
};

struct ConfigEntryResult {
  bool applied{false};  // false = ignored (unknown key or invalid value); config unchanged
  char warning[kMaxConfigWarningLen]{};
};

ConfigEntryResult apply_config_entry(AppConfig& config, const char* key, const char* value);

}  // namespace ledwall
