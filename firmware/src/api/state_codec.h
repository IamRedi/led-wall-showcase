#pragma once

#include <ArduinoJson.h>

#include "api/identity.h"
#include "core/public_state.h"

namespace ledwall {

// Wire serialization of the §4 public state and §10 identity payloads
// (ArduinoJson). Shared by the HTTP endpoints and the WebSocket broadcaster.
// wifi.password and all other secrets are never serialized.
void serialize_identity(const Identity& id, JsonDocument& doc);

void serialize_state(const PublicState& state, const Identity& id, JsonVariant doc);

// Typed error envelope (§9): {"ok":false,"error":{"code","message","available"?}}
void serialize_error(ErrorCode code, const char* message, const char* available,
                     JsonDocument& doc);

}  // namespace ledwall
