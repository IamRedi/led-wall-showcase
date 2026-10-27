#include "api/state_codec.h"

#include <cstdio>
#include <cstring>

#include "text/text_style.h"

namespace ledwall {

namespace {
const char* mode_kind_wire(ModeKind kind) { return mode_name(kind); }
}  // namespace

void serialize_identity(const Identity& id, JsonDocument& doc) {
  doc["deviceId"] = id.device_id;
  doc["deviceType"] = id.device_type;
  doc["role"] = id.role;
  doc["protocolVersion"] = id.protocol_version;
  doc["fwVersion"] = id.fw_version;
  JsonArray caps = doc["capabilities"].to<JsonArray>();
  for (uint8_t i = 0; i < id.capability_count; ++i) {
    caps.add(id.capabilities[i]);
  }
  JsonObject matrix = doc["matrix"].to<JsonObject>();
  matrix["width"] = id.matrix_width;
  matrix["height"] = id.matrix_height;
}

void serialize_state(const PublicState& state, const Identity& id, JsonVariant doc) {
  doc["available"] = state.available;
  doc["deviceId"] = state.device_id;
  doc["deviceType"] = state.device_type;
  doc["protocolVersion"] = state.protocol_version;
  doc["fwVersion"] = state.fw_version;

  doc["baseMode"] = mode_kind_wire(state.base_mode);
  doc["currentMode"] = mode_kind_wire(state.current_mode);

  if (state.transient_active) {
    JsonObject transient = doc["activeTransient"].to<JsonObject>();
    transient["kind"] = mode_kind_wire(state.transient_kind);
    transient["remainingMs"] = state.transient_remaining_ms;
  } else {
    doc["activeTransient"] = nullptr;
  }

  // §4: activeScene/activeAnimation/activeExpression — the base program name
  // when the base is that kind, null otherwise.
  if (state.base_mode == ModeKind::Scene) {
    doc["activeScene"] = state.base_name;
  } else {
    doc["activeScene"] = nullptr;
  }
  if (state.base_mode == ModeKind::Animation) {
    doc["activeAnimation"] = state.base_name;
  } else {
    doc["activeAnimation"] = nullptr;
  }
  if (state.base_mode == ModeKind::Expression) {
    doc["activeExpression"] = state.base_name;
  } else {
    doc["activeExpression"] = nullptr;
  }

  if (state.base_mode == ModeKind::Text || state.current_mode == ModeKind::Text) {
    JsonObject text = doc["text"].to<JsonObject>();
    text["current"] = state.text_current;
    text["scrolling"] = state.text_scrolling;
    // LED-L009 Text Studio view (additive).
    text["mode"] = text_mode_name(state.text_mode);
    text["rtl"] = state.text_rtl;
    text["loop"] = state.text_loop;
    text["wordDwellMs"] = state.text_word_dwell_ms;
    text["style"] = text::text_style_name(state.text_style);
    text["color"] = text::text_palette_name(state.text_fg_color);
    text["bgColor"] = text::text_palette_name(state.text_bg_color);
  }

  JsonObject brightness = doc["brightness"].to<JsonObject>();
  brightness["current"] = state.brightness_current;
  brightness["max"] = state.brightness_max;

  JsonObject spectrum = doc["spectrum"].to<JsonObject>();
  spectrum["available"] = state.spectrum_available;
  spectrum["active"] = state.spectrum_active;

  // LED-L009 device-owned subsystems.
  JsonObject timer = doc["timer"].to<JsonObject>();
  timer["state"] = CountdownTimer::state_name(state.timer_state);
  timer["durationMs"] = state.timer_duration_ms;
  timer["remainingMs"] = state.timer_remaining_ms;

  JsonObject menu = doc["menu"].to<JsonObject>();
  menu["active"] = state.menu_active;

  JsonObject game = doc["game"].to<JsonObject>();
  game["active"] = state.game_active;
  game["name"] = state.game_name;
  game["phase"] = state.game_phase;
  game["score"] = state.game_score;
  game["scoreP1"] = state.game_score;
  game["scoreP2"] = state.game_score_p2;
  game["winner"] = state.game_winner == 1 ? "p1" :
                   state.game_winner == 2 ? "p2" :
                   state.game_winner == 3 ? "draw" : "none";
  if (state.game_high_score_available) game["highScore"] = state.game_high_score;
  else game["highScore"] = nullptr;
  game["difficulty"] = state.game_difficulty;  // V-L009-3
  game["players"] = state.game_players;        // LED-L010 engine player mode

  JsonObject standby = doc["standby"].to<JsonObject>();
  standby["on"] = state.standby_on;

  JsonObject notification = doc["notification"].to<JsonObject>();
  notification["active"] = state.notification_active;
  notification["high"] = state.notification_high;
  if (state.notification_active) {
    notification["text"] = state.notification_text;
  }

  JsonObject visualizer = doc["visualizer"].to<JsonObject>();
  visualizer["active"] = state.visualizer_active;
  JsonObject sound = doc["sound"].to<JsonObject>();
  sound["muted"] = state.sound_muted;
  sound["physicalOutputAvailable"] = false;

  JsonObject matrix = doc["matrix"].to<JsonObject>();
  matrix["width"] = state.matrix_width;
  matrix["height"] = state.matrix_height;

  // LED-L010 zone truth (additive).
  JsonObject tail = doc["tail"].to<JsonObject>();
  tail["enabled"] = state.tail_enabled;
  tail["length"] = state.tail_length;
  tail["reverse"] = state.tail_reverse;
  if (state.tail_scan_index >= 0) {
    tail["scanIndex"] = static_cast<uint16_t>(state.tail_scan_index);
  } else {
    tail["scanIndex"] = nullptr;
  }
  if (state.tail_setup_index >= 0) {
    tail["setupIndex"] = static_cast<uint16_t>(state.tail_setup_index);
  } else {
    tail["setupIndex"] = nullptr;
  }
  if (state.base_mode == ModeKind::Scene || state.base_mode == ModeKind::Animation ||
      state.current_mode == ModeKind::Scene || state.current_mode == ModeKind::Animation) {
    doc["animationTarget"] = state.animation_target;
  } else {
    doc["animationTarget"] = nullptr;
  }

  // LED-L010 DS4 controller view (supported only in the esp32_eq_ds4 build).
  JsonObject ds4 = doc["ds4"].to<JsonObject>();
  ds4["supported"] = state.ds4_supported;
  ds4["enabled"] = state.ds4_enabled;
  ds4["state"] = state.ds4_state;
  ds4["players"] = state.ds4_players;
#ifdef LED_WALL_ENABLE_DS4
  ds4["ready"] = state.ds4_ready;
  ds4["assignedMask"] = state.ds4_assigned_mask;
  ds4["inputEvents"] = state.ds4_input_events;
#endif
  JsonArray ds4Battery = ds4["battery"].to<JsonArray>();
  ds4Battery.add(state.ds4_battery[0]);
  ds4Battery.add(state.ds4_battery[1]);

  JsonObject clients = doc["clients"].to<JsonObject>();
  clients["count"] = state.clients_count;

  JsonArray warnings = doc["warnings"].to<JsonArray>();
  for (uint8_t i = 0; i < state.warning_count; ++i) {
    warnings.add(state.warnings[i]);
  }

  doc["uptimeSeconds"] = state.uptime_seconds;
  (void)id;  // identity is embedded by the /identity endpoint; state carries its own fields
}

void serialize_error(ErrorCode code, const char* message, const char* available,
                     JsonDocument& doc) {
  doc["ok"] = false;
  JsonObject error = doc["error"].to<JsonObject>();
  error["code"] = error_code_name(code);
  if (message != nullptr) {
    error["message"] = message;
  }
  if (available != nullptr && available[0] != '\0') {
    error["available"] = available;
  }
}

}  // namespace ledwall
