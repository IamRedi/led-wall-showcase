#pragma once

#include <cstddef>
#include <cstdint>

#include "games/game.h"
#include "render/display_topology.h"

namespace ledwall {

// Registries (frozen spec §7/§8 + LED-L009 additive extension, ADR-LED-L009).
// Registry names are the only scene/animation/expression/game handles; legacy
// function names are never API names. Adding entries never changes the API
// shape. The first 8 animation names and first 8 expression names are the
// frozen initial sets in their original order — accepted clients keep working
// (newer names are discoverable via GET /registry, LED-L009).
//
// §7: "animations = motion effects usable as scenes or expression backdrops"
// — animation-class entries are usable as scenes and reported as scenes.
//
// LED-L009 animation additions (curated, documented in CONTRACT.md):
//   icon set:  smile, wink, heart, heart_pulse, eyes, loading, success,
//              warning, arrows
//   ambient:   aurora, stars, rainbow_flow, fire, matrix, calm
// LED-L009 expression additions (§16 HUGO expression library): bored, tired,
// focused, excited — mapped data-driven like the frozen eight.

constexpr const char* const kExpressionNames[] = {
    "idle", "listening", "thinking", "speaking", "happy", "alert", "error", "sleeping",
    "bored", "tired", "focused", "excited",
};
constexpr uint8_t kExpressionCount =
    static_cast<uint8_t>(sizeof(kExpressionNames) / sizeof(kExpressionNames[0]));
// Frozen §8 vocabulary boundary (additive compatibility marker).
constexpr uint8_t kFrozenExpressionCount = 8;

constexpr const char* const kAnimationNames[] = {
    "corners", "checker", "border", "row_sweep", "bounce", "wave", "rain", "plasma",
    "smile", "wink", "heart", "heart_pulse", "eyes", "loading", "success", "warning",
    "arrows", "aurora", "stars", "rainbow_flow", "fire", "matrix", "calm",
    // LED-L010 tail/combined expansion (ADR-LED-L010 D3; appended additively —
    // the frozen first-8 and the LED-L009 sets keep their original order):
    // 1D-native tail effects + both-surface combined effects.
    "comet", "scanner", "pulse", "breathing", "meteor", "spark_trail", "lightning",
    "fire_flow", "progress", "countdown_progress", "chase", "color_wave", "heartbeat",
    "fireworks", "meteor_impact", "energy_bloom", "rainbow_river", "storm",
    "lava_fountain", "rain_drain", "aurora_current", "starstream",
};
constexpr uint8_t kAnimationCount =
    static_cast<uint8_t>(sizeof(kAnimationNames) / sizeof(kAnimationNames[0]));
// Frozen §7 registry boundary (additive compatibility marker).
constexpr uint8_t kFrozenAnimationCount = 8;

// LED-L010 animation zone metadata (ADR-LED-L010 D3): supported-target masks
// parallel to kAnimationNames (same index order). Existing names are
// matrix-only except `rainbow_flow`, which gains tail+both (default matrix —
// byte-compatible with every existing client). Tail-only: comet, scanner,
// meteor, spark_trail, fire_flow. Tail+both: pulse, breathing, lightning,
// progress, countdown_progress, chase, color_wave, heartbeat.
uint8_t animation_zone_mask(const char* name);
ZoneTarget animation_default_target(const char* name);

// LED-L010 game metadata (ADR-LED-L010 D6): display surface and supported
// player counts, parallel to kGameNames.
constexpr uint8_t kGameMaxPlayers2 = 2;

// LED-L009 games registry (curated five; §14 of the reconstruction brief).
// LED-L010 appends the six tail-native games (ADR-LED-L010 D6) additively.
constexpr const char* const kGameNames[] = {
    "pong", "snake", "breakout", "reaction", "dodger",
    "color_chain", "light_race", "reaction_stop", "color_catch", "memory",
    "tug_of_war", "tail_tennis", "light_quest", "quick_draw",
};
constexpr uint8_t kGameCount =
    static_cast<uint8_t>(sizeof(kGameNames) / sizeof(kGameNames[0]));

// LED-L010 game surface metadata: the zone the game's PRIMARY gameplay renders
// on ("matrix" for the five accepted games; "tail" for reaction_stop; "both"
// where the tail gameplay carries a matrix HUD/scoreboard). Discovery via
// GET /registry.
ZoneTarget game_zone(const char* name);
// LED-L010 supported player counts: 1 = solo only; 2 = 1P and 2P (Pong, the
// two duels). The player mode applies at the next start/restart (like
// difficulty, V-L009-3).
uint8_t game_max_players(const char* name);
uint8_t game_min_players(const char* name);

// Repair cycle 1 (M-L010-2/3): minimum configured tail lane (logical cells)
// the game supports. The six tail games require games::kTailGameMinLane (12);
// matrix games require no lane (0). The dispatcher rejects game.set for a
// tail game whose configured tail.length is below this bound with a typed
// invalidParameter (an explicit supported-length rule instead of silently
// running on a substituted lane).
uint16_t game_min_lane(const char* name);

// LED-L009 unified input event names (wire vocabulary for display.input).
constexpr const char* const kInputEventNames[] = {
    "up", "down", "left", "right", "confirm", "back", "start_pause", "home", "action_b",
};
constexpr uint8_t kInputEventCount =
    static_cast<uint8_t>(sizeof(kInputEventNames) / sizeof(kInputEventNames[0]));

bool valid_expression(const char* name);
bool valid_animation(const char* name);
bool valid_scene(const char* name);  // animation-class entries are usable as scenes (§7)
bool valid_game(const char* name);
bool valid_input_event(const char* name);

// Comma-joined available-name list for unknownX error responses (§9).
// Returns false if the buffer was too small (truncated output, still NUL-terminated).
bool list_names(const char* const* names, uint8_t count, char* out, size_t cap);

}  // namespace ledwall
