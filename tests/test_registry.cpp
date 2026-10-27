#include "api/registry.h"
#include "test_framework.h"

// Frozen registries (spec §7/§8): the 8-expression vocabulary and the 8
// animation-class keys usable as scenes. Expectations are derived from the
// frozen baseline, not from the implementation.

void test_registry_expression_vocabulary() {
  const char* frozen[8] = {"idle", "listening", "thinking", "speaking",
                           "happy", "alert", "error", "sleeping"};
  // LED-L009: the frozen §8 set stays intact in original order; four curated
  // intents are appended additively (ADR-LED-L009).
  CHECK_EQ_INT(ledwall::kFrozenExpressionCount, 8);
  for (uint8_t i = 0; i < ledwall::kFrozenExpressionCount; ++i) {
    CHECK(std::strcmp(ledwall::kExpressionNames[i], frozen[i]) == 0);
  }
  CHECK_EQ_INT(ledwall::kExpressionCount, 12);
  const char* added[4] = {"bored", "tired", "focused", "excited"};
  for (uint8_t i = 0; i < 4; ++i) {
    CHECK(std::strcmp(ledwall::kExpressionNames[8 + i], added[i]) == 0);
  }
  for (const char* name : frozen) {
    ++g_checks;
    if (!ledwall::valid_expression(name)) {
      ++g_failures;
      std::printf("FAIL: expression '%s' must be valid (frozen §8)\n", name);
    }
  }
  CHECK(!ledwall::valid_expression("bogus"));
  CHECK(!ledwall::valid_expression("HAPPY"));  // exact frozen names, case-sensitive
  CHECK(!ledwall::valid_expression(""));
  CHECK(!ledwall::valid_expression(nullptr));
}

void test_registry_animation_keys() {
  const char* frozen[8] = {"corners", "checker", "border", "row_sweep",
                           "bounce", "wave", "rain", "plasma"};
  // LED-L009: frozen §7 keys stay intact in original order; curated additions
  // appended (icon set + ambient set, ADR-LED-L009).
  CHECK_EQ_INT(ledwall::kFrozenAnimationCount, 8);
  for (uint8_t i = 0; i < ledwall::kFrozenAnimationCount; ++i) {
    CHECK(std::strcmp(ledwall::kAnimationNames[i], frozen[i]) == 0);
  }
  CHECK_EQ_INT(ledwall::kAnimationCount, 45);
  const char* added[15] = {"smile", "wink", "heart", "heart_pulse", "eyes",
                           "loading", "success", "warning", "arrows", "aurora",
                           "stars", "rainbow_flow", "fire", "matrix", "calm"};
  for (uint8_t i = 0; i < 15; ++i) {
    CHECK(std::strcmp(ledwall::kAnimationNames[8 + i], added[i]) == 0);
  }
  // LED-L010: the tail/combined set is appended additively (ADR-LED-L010 D3).
  const char* tail_added[13] = {"comet", "scanner", "pulse", "breathing", "meteor",
                                "spark_trail", "lightning", "fire_flow", "progress",
                                "countdown_progress", "chase", "color_wave", "heartbeat"};
  for (uint8_t i = 0; i < 13; ++i) {
    CHECK(std::strcmp(ledwall::kAnimationNames[23 + i], tail_added[i]) == 0);
  }
  // LED-L010 zone metadata: rainbow_flow gained tail/both; the others match
  // the documented default-target table.
  CHECK(ledwall::animation_zone_mask("rainbow_flow") ==
        (ledwall::zone_flags::kMatrix | ledwall::zone_flags::kTail | ledwall::zone_flags::kBoth));
  CHECK(ledwall::animation_zone_mask("corners") == ledwall::zone_flags::kMatrix);
  CHECK(ledwall::animation_zone_mask("comet") == ledwall::zone_flags::kTail);
  CHECK(ledwall::animation_zone_mask("pulse") ==
        (ledwall::zone_flags::kTail | ledwall::zone_flags::kBoth));
  CHECK(ledwall::animation_default_target("rainbow_flow") == ledwall::ZoneTarget::Matrix);
  CHECK(ledwall::animation_default_target("comet") == ledwall::ZoneTarget::Tail);
  CHECK(ledwall::animation_default_target("wave") == ledwall::ZoneTarget::Matrix);
  CHECK(ledwall::valid_animation("fireworks"));
  CHECK(ledwall::animation_zone_mask("fireworks") == ledwall::zone_flags::kBoth);
  CHECK(ledwall::animation_default_target("fireworks") == ledwall::ZoneTarget::Both);
  const char* v41_added[9] = {"fireworks", "meteor_impact", "energy_bloom",
                               "rainbow_river", "storm", "lava_fountain",
                               "rain_drain", "aurora_current", "starstream"};
  for (uint8_t i = 0; i < 9; ++i) {
    CHECK(std::strcmp(ledwall::kAnimationNames[36 + i], v41_added[i]) == 0);
    CHECK(ledwall::animation_zone_mask(v41_added[i]) == ledwall::zone_flags::kBoth);
    CHECK(ledwall::animation_default_target(v41_added[i]) == ledwall::ZoneTarget::Both);
  }
  CHECK(ledwall::zone_target_supported(ledwall::animation_zone_mask("pulse"),
                                       ledwall::ZoneTarget::Both));
  CHECK(!ledwall::zone_target_supported(ledwall::animation_zone_mask("wave"),
                                        ledwall::ZoneTarget::Tail));
  for (const char* name : frozen) {
    ++g_checks;
    if (!ledwall::valid_animation(name)) {
      ++g_failures;
      std::printf("FAIL: animation '%s' must be valid (frozen §7)\n", name);
    }
  }
  CHECK(!ledwall::valid_animation("demoRain"));  // legacy function names are never API names
  CHECK(!ledwall::valid_animation("Rain"));
  CHECK(!ledwall::valid_animation(nullptr));
}

void test_registry_scene_usable_animations() {
  // §7: animations are "usable as scenes"; no dedicated scene entries yet.
  CHECK(ledwall::valid_scene("wave"));
  CHECK(!ledwall::valid_scene("bogus"));
}

void test_registry_games_and_inputs() {
  // LED-L009 games registry (brief §14: five polished classics).
  CHECK_EQ_INT(ledwall::kGameCount, 14);
  const char* games[11] = {"pong", "snake", "breakout", "reaction", "dodger",
                           // LED-L010 tail lane games (ADR-LED-L010 D6).
                           "color_chain", "light_race", "reaction_stop", "color_catch",
                           "memory", "tug_of_war"};
  for (uint8_t i = 0; i < 11; ++i) {
    CHECK(std::strcmp(ledwall::kGameNames[i], games[i]) == 0);
    CHECK(ledwall::valid_game(games[i]));
  }
  CHECK(!ledwall::valid_game("tetris"));
  CHECK(!ledwall::valid_game(nullptr));
  // LED-L010 game metadata: surfaces + supported player counts.
  CHECK(ledwall::game_zone("pong") == ledwall::ZoneTarget::Matrix);
  CHECK(ledwall::game_zone("reaction_stop") == ledwall::ZoneTarget::Tail);
  CHECK(ledwall::game_zone("color_chain") == ledwall::ZoneTarget::Both);
  CHECK(ledwall::game_max_players("pong") == 2);
  CHECK(ledwall::game_max_players("light_race") == 2);
  CHECK(ledwall::game_max_players("tug_of_war") == 2);
  CHECK(ledwall::game_max_players("snake") == 1);
  CHECK(ledwall::game_max_players("nope") == 1);  // fail closed
  // Unified input wire vocabulary.
  CHECK_EQ_INT(ledwall::kInputEventCount, 9);
  const char* inputs[8] = {"up", "down", "left", "right", "confirm", "back", "start_pause", "home"};
  for (uint8_t i = 0; i < 8; ++i) {
    CHECK(std::strcmp(ledwall::kInputEventNames[i], inputs[i]) == 0);
    CHECK(ledwall::valid_input_event(inputs[i]));
  }
  CHECK(!ledwall::valid_input_event("warp"));
}

void test_registry_available_list() {
  char buf[320];
  CHECK(ledwall::list_names(ledwall::kExpressionNames, ledwall::kExpressionCount, buf, sizeof(buf)));
  CHECK(ledwall::list_names(ledwall::kExpressionNames, ledwall::kExpressionCount, buf, sizeof(buf)));
  // Derived independently from the full registry (LED-L009: 12 names).
  CHECK(std::strcmp(buf,
                    "idle, listening, thinking, speaking, happy, alert, error, sleeping, "
                    "bored, tired, focused, excited") == 0);

  char small[12];
  // Truncation policy: the buffer retains the last complete joined list
  // ("corners"), NUL-terminated, and the function reports false.
  const bool complete = ledwall::list_names(ledwall::kAnimationNames, ledwall::kAnimationCount,
                                            small, sizeof(small));
  CHECK(!complete);
  CHECK(std::strcmp(small, "corners") == 0);
}
