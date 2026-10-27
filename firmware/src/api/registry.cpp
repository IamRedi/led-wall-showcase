#include "api/registry.h"

#include <cstring>

namespace ledwall {

namespace {
bool contains(const char* const* names, uint8_t count, const char* name) {
  if (name == nullptr) {
    return false;
  }
  for (uint8_t i = 0; i < count; ++i) {
    if (std::strcmp(name, names[i]) == 0) {
      return true;
    }
  }
  return false;
}
}  // namespace

bool valid_expression(const char* name) { return contains(kExpressionNames, kExpressionCount, name); }

bool valid_animation(const char* name) { return contains(kAnimationNames, kAnimationCount, name); }

bool valid_game(const char* name) { return contains(kGameNames, kGameCount, name); }

bool valid_input_event(const char* name) {
  return contains(kInputEventNames, kInputEventCount, name);
}

bool valid_scene(const char* name) {
  // §7: animations are usable as scenes; no dedicated scene-class entries exist yet.
  return valid_animation(name);
}

// LED-L010 zone metadata (parallel to kAnimationNames; see registry.h). A name
// not found in the mask table (cannot happen for registry names) is
// matrix-only: fail closed toward the accepted behavior.
uint8_t animation_zone_mask(const char* name) {
  if (name == nullptr) {
    return zone_flags::kMatrix;
  }
  struct ZoneBinding {
    const char* name;
    uint8_t mask;
  };
  static constexpr ZoneBinding kZones[] = {
      {"rainbow_flow", zone_flags::kMatrix | zone_flags::kTail | zone_flags::kBoth},
      {"comet", zone_flags::kTail},
      {"scanner", zone_flags::kTail},
      {"pulse", zone_flags::kTail | zone_flags::kBoth},
      {"breathing", zone_flags::kTail | zone_flags::kBoth},
      {"meteor", zone_flags::kTail},
      {"spark_trail", zone_flags::kTail},
      {"lightning", zone_flags::kTail | zone_flags::kBoth},
      {"fire_flow", zone_flags::kTail},
      {"progress", zone_flags::kTail | zone_flags::kBoth},
      {"countdown_progress", zone_flags::kTail | zone_flags::kBoth},
      {"chase", zone_flags::kTail | zone_flags::kBoth},
      {"color_wave", zone_flags::kTail | zone_flags::kBoth},
      {"heartbeat", zone_flags::kTail | zone_flags::kBoth},
      {"fireworks", zone_flags::kBoth},
      {"meteor_impact", zone_flags::kBoth},
      {"energy_bloom", zone_flags::kBoth},
      {"rainbow_river", zone_flags::kBoth},
      {"storm", zone_flags::kBoth},
      {"lava_fountain", zone_flags::kBoth},
      {"rain_drain", zone_flags::kBoth},
      {"aurora_current", zone_flags::kBoth},
      {"starstream", zone_flags::kBoth},
  };
  for (const ZoneBinding& z : kZones) {
    if (std::strcmp(z.name, name) == 0) {
      return z.mask;
    }
  }
  return zone_flags::kMatrix;
}

ZoneTarget animation_default_target(const char* name) {
  // Only rainbow_flow's default differs from its registry history (Matrix);
  // every tail-native name defaults to Tail, both-capable names to Tail as
  // well (the combined form is explicitly requested with target:"both").
  if (name != nullptr && std::strcmp(name, "rainbow_flow") == 0) {
    return ZoneTarget::Matrix;
  }
  if (animation_zone_mask(name) == zone_flags::kBoth) {
    return ZoneTarget::Both;
  }
  const uint8_t mask = animation_zone_mask(name);
  if (mask & zone_flags::kTail) {
    return ZoneTarget::Tail;
  }
  return ZoneTarget::Matrix;
}

// LED-L010 game metadata (parallel to kGameNames; see registry.h). Unknown
// names fail closed to the accepted matrix/solo shape.
ZoneTarget game_zone(const char* name) {
  if (name == nullptr) {
    return ZoneTarget::Matrix;
  }
  struct GameZone {
    const char* name;
    ZoneTarget zone;
  };
  static constexpr GameZone kZones[] = {
      {"color_chain", ZoneTarget::Both},
      {"light_race", ZoneTarget::Both},
      {"reaction_stop", ZoneTarget::Tail},
      {"color_catch", ZoneTarget::Both},
      {"memory", ZoneTarget::Both},
      {"tug_of_war", ZoneTarget::Both},
      {"tail_tennis", ZoneTarget::Both},
      {"light_quest", ZoneTarget::Both},
      {"quick_draw", ZoneTarget::Both},
  };
  for (const GameZone& g : kZones) {
    if (std::strcmp(g.name, name) == 0) {
      return g.zone;
    }
  }
  return ZoneTarget::Matrix;
}

uint8_t game_max_players(const char* name) {
  if (name != nullptr && (std::strcmp(name, "pong") == 0 ||
                          std::strcmp(name, "light_race") == 0 ||
                          std::strcmp(name, "tug_of_war") == 0 ||
                          std::strcmp(name, "tail_tennis") == 0 ||
                          std::strcmp(name, "reaction_stop") == 0 ||
                          std::strcmp(name, "quick_draw") == 0 ||
                          std::strcmp(name, "memory") == 0 ||
                          std::strcmp(name, "color_catch") == 0)) {
    return 2;
  }
  return 1;
}

uint8_t game_min_players(const char* name) {
  return name != nullptr && (std::strcmp(name, "light_race") == 0 ||
                             std::strcmp(name, "quick_draw") == 0) ? 2 : 1;
}

uint16_t game_min_lane(const char* name) {
  // Repair cycle 1 (M-L010-2/3): tail lane games are rejected below the
  // shared playable minimum (games::kTailGameMinLane) — an explicit
  // supported-length rule instead of silently running on a substituted lane.
  if (name != nullptr && std::strcmp(name, "color_chain") == 0) {
    return 30;  // 30 grouped game cells; shorter lanes cannot render them.
  }
  if (name != nullptr && (std::strcmp(name, "light_race") == 0 ||
                          std::strcmp(name, "reaction_stop") == 0 ||
                          std::strcmp(name, "color_catch") == 0 ||
                          std::strcmp(name, "memory") == 0 ||
                          std::strcmp(name, "tug_of_war") == 0 ||
                          std::strcmp(name, "tail_tennis") == 0 ||
                          std::strcmp(name, "light_quest") == 0 ||
                          std::strcmp(name, "quick_draw") == 0)) {
    return games::kTailGameMinLane;
  }
  return 0;
}

bool list_names(const char* const* names, uint8_t count, char* out, size_t cap) {
  if (out == nullptr || cap == 0) {
    return false;
  }
  out[0] = '\0';
  size_t used = 0;
  for (uint8_t i = 0; i < count; ++i) {
    const size_t len = std::strlen(names[i]);
    const size_t extra = len + ((i == 0) ? 0u : 2u);
    if (used + extra + 1 > cap) {
      return false;  // truncated (out remains valid/NUL-terminated from previous write)
    }
    if (i != 0) {
      out[used] = ',';
      out[used + 1] = ' ';
      used += 2;
    }
    std::memcpy(out + used, names[i], len);
    used += len;
    out[used] = '\0';
  }
  return true;
}

}  // namespace ledwall
