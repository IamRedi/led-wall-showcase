#include "api/http_server.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#ifndef LED_WALL_ENABLE_DS4
#include <SPIFFS.h>
#endif
#include <esp_system.h>

#include <cstring>

#include "api/state_codec.h"
#include "api/ui_content.h"
#include "text/text_style.h"
#ifdef LED_WALL_ENABLE_DS4
#include "input/ds4_provider_bluepad32.h"
#endif

namespace ledwall {

namespace {

bool pixel_project_name(const char* name) {
  if (name == nullptr) return false;
  const size_t n = std::strlen(name);
  if (n == 0 || n > 24) return false;
  for (size_t i = 0; i < n; ++i) {
    const char ch = name[i];
    if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
          (ch >= '0' && ch <= '9') || ch == '_' || ch == '-')) return false;
  }
  return true;
}

bool pixel_hex(const char* pixels) {
  if (pixels == nullptr || std::strlen(pixels) != PixelCanvas::kHexChars) return false;
  for (size_t i = 0; i < PixelCanvas::kHexChars; ++i) {
    const char c = pixels[i];
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
          (c >= 'A' && c <= 'F'))) return false;
  }
  return true;
}

bool parse_custom_scene(JsonObject obj, SceneSpec* out) {
  if (out == nullptr || obj.size() != 3 || !obj["name"].is<const char*>() ||
      !pixel_project_name(obj["name"].as<const char*>()) ||
      !obj["loop"].is<bool>() || !obj["steps"].is<JsonArray>()) return false;
  SceneSpec spec;
  std::strncpy(spec.name, obj["name"].as<const char*>(), sizeof(spec.name) - 1);
  spec.loop = obj["loop"].as<bool>();
  JsonArray steps = obj["steps"].as<JsonArray>();
  if (steps.size() == 0 || steps.size() > 8) return false;
  for (JsonVariant v : steps) {
    if (!v.is<JsonObject>()) return false;
    JsonObject row = v.as<JsonObject>();
    if (!row["kind"].is<const char*>() || !row["durationMs"].is<uint32_t>()) return false;
    SceneStep& step = spec.steps[spec.count];
    const char* kind = row["kind"].as<const char*>();
    if (std::strcmp(kind, "text") == 0) {
      if (row.size() != 3 || !row["text"].is<const char*>() ||
          std::strlen(row["text"].as<const char*>()) >= sizeof(step.content)) return false;
      step.kind = SceneStepKind::Text;
      std::strncpy(step.content, row["text"].as<const char*>(), sizeof(step.content) - 1);
    } else if (std::strcmp(kind, "effect") == 0) {
      if (row.size() != 3 || !row["effect"].is<const char*>() ||
          std::strlen(row["effect"].as<const char*>()) >= sizeof(step.content)) return false;
      step.kind = SceneStepKind::Effect;
      std::strncpy(step.content, row["effect"].as<const char*>(), sizeof(step.content) - 1);
    } else if (std::strcmp(kind, "wait") == 0) {
      if (row.size() != 2) return false;
      step.kind = SceneStepKind::Wait;
    } else return false;
    step.duration_ms = row["durationMs"].as<uint32_t>();
    ++spec.count;
  }
  SceneTimeline candidate;
  if (!candidate.start(spec, 0)) return false;
  *out = spec;
  return true;
}

#ifndef LED_WALL_ENABLE_DS4
String pixel_path(const char* name) { return String("/px_") + name + ".rgb"; }
String pixel_backup(const char* name) { return String("/px_") + name + ".bak"; }
String scene_path(const char* name) { return String("/sc_") + name + ".json"; }
String scene_backup(const char* name) { return String("/sc_") + name + ".bak"; }
bool pixel_exists(const char* name) {
  return SPIFFS.exists(pixel_path(name)) || SPIFFS.exists(pixel_backup(name));
}
uint8_t pixel_project_count() {
  File root = SPIFFS.open("/");
  if (!root) return 0;
  uint8_t count = 0;
  for (File file = root.openNextFile(); file; file = root.openNextFile()) {
    const String name = file.name();
    if (!name.startsWith("/px_") || name == "/px_pending.rgb") continue;
    if (name.endsWith(".rgb")) ++count;
    else if (name.endsWith(".bak") &&
             !SPIFFS.exists(name.substring(0, name.length() - 4) + ".rgb")) ++count;
  }
  return count;
}
uint8_t scene_project_count() {
  File root = SPIFFS.open("/");
  if (!root) return 0;
  uint8_t count = 0;
  for (File file = root.openNextFile(); file; file = root.openNextFile()) {
    const String path = file.name();
    if (!path.startsWith("/sc_") || path == "/sc_pending.json") continue;
    if (path.endsWith(".json")) ++count;
    else if (path.endsWith(".bak") &&
             !SPIFFS.exists(path.substring(0, path.length() - 4) + ".json")) ++count;
  }
  return count;
}
#endif

CommandResponse param_error(const char* message) {
  CommandResponse r;
  r.ok = false;
  r.error = ErrorCode::InvalidParameter;
  std::strncpy(r.message, message, kMaxErrorMessageLen - 1);
  r.message[kMaxErrorMessageLen - 1] = '\0';
  return r;
}

}  // namespace

void HttpApi::begin(uint16_t port, CommandDispatcher* dispatcher, IdentityFn identity_fn,
                    StateFn state_fn, PixelSessionFn pixel_session_fn,
                    PixelFrameFn pixel_frame_fn, ScenePlayFn scene_play_fn,
                    SceneStopFn scene_stop_fn, InputEventQueue* input_queue,
                    SoundMuteFn sound_mute_fn) {
  dispatcher_ = dispatcher;
  identity_fn_ = identity_fn;
  state_fn_ = state_fn;
  pixel_session_fn_ = pixel_session_fn;
  pixel_frame_fn_ = pixel_frame_fn;
  scene_play_fn_ = scene_play_fn;
  scene_stop_fn_ = scene_stop_fn;
  input_queue_ = input_queue;
  sound_mute_fn_ = sound_mute_fn;
#ifndef LED_WALL_ENABLE_DS4
  // Existing default partition only; never format an unknown/old filesystem.
  pixel_store_ready_ = SPIFFS.begin(false);
#endif

  server_ = new WebServer(port);
  // ESP32 WebServer handlers are std::function — lambdas capturing `this` are
  // fine for the single firmware instance.
  server_->on("/display/text/show", HTTP_POST, [this]() { handle_command_(SemanticCommand::Capability::TextShow); });
  server_->on("/display/text/clear", HTTP_POST, [this]() { handle_command_(SemanticCommand::Capability::TextClear); });
  server_->on("/display/scene/set", HTTP_POST, [this]() { handle_command_(SemanticCommand::Capability::SceneSet); });
  server_->on("/display/animation/set", HTTP_POST, [this]() { handle_command_(SemanticCommand::Capability::AnimationSet); });
  server_->on("/display/expression/set", HTTP_POST, [this]() { handle_command_(SemanticCommand::Capability::ExpressionSet); });
  server_->on("/display/clear", HTTP_POST, [this]() { handle_command_(SemanticCommand::Capability::Clear); });
  server_->on("/display/brightness/set", HTTP_POST, [this]() { handle_command_(SemanticCommand::Capability::BrightnessSet); });
  server_->on("/display/tail/point", HTTP_POST, [this]() { handle_command_(SemanticCommand::Capability::TailPoint); });
  server_->on("/display/tail/off", HTTP_POST, [this]() { handle_command_(SemanticCommand::Capability::TailPointOff); });
  server_->on("/display/pixel/session", HTTP_POST, [this]() { handle_pixel_session_(); });
  server_->on("/display/pixel/frame", HTTP_POST, [this]() { handle_pixel_frame_(); });
  server_->on("/display/pixel/projects", HTTP_GET, [this]() { handle_pixel_projects_(); });
  server_->on("/display/pixel/project/save", HTTP_POST, [this]() { handle_pixel_project_save_(); });
  server_->on("/display/pixel/project/load", HTTP_POST, [this]() { handle_pixel_project_load_(); });
  server_->on("/display/party/play", HTTP_POST, [this]() { handle_scene_play_(); });
  server_->on("/display/party/projects", HTTP_GET, [this]() { handle_scene_projects_(); });
  server_->on("/display/party/project/save", HTTP_POST, [this]() { handle_scene_project_save_(); });
  server_->on("/display/party/project/load", HTTP_POST, [this]() { handle_scene_project_load_(); });
  server_->on("/display/party/stop", HTTP_POST, [this]() {
    if (scene_stop_fn_ != nullptr) scene_stop_fn_(millis());
    server_->send(200, "application/json", "{\"ok\":true,\"stopped\":true}");
  });
  server_->on("/display/input/session", HTTP_POST, [this]() { handle_input_session_(); });
  server_->on("/display/input/event", HTTP_POST, [this]() { handle_input_event_(); });
  server_->on("/display/input/heartbeat", HTTP_POST, [this]() { handle_input_heartbeat_(); });
  server_->on("/display/input/release", HTTP_POST, [this]() { handle_input_release_(); });
#ifdef LED_WALL_ENABLE_DS4
  server_->on("/display/controller/pair", HTTP_POST, [this]() {
    server_->send(ds4::pair() ? 200 : 409, "application/json",
                   ds4::view().ready ? "{\"ok\":true}" : "{\"ok\":false,\"error\":{\"code\":\"notReady\"}}");
  });
  server_->on("/display/controller/assign", HTTP_POST, [this]() {
    JsonDocument doc;
    if (!parse_body_(doc) || !doc.is<JsonObject>() || doc.as<JsonObject>().size() != 1 ||
        !doc["player"].is<int>() || doc["player"].as<int>() < 1 || doc["player"].as<int>() > 2) {
      send_error_(param_error("player 1 or 2 required")); return;
    }
    const uint8_t player = static_cast<uint8_t>(doc["player"].as<int>());
    if (input_sessions_.claimed(player) || !ds4::assign_primary(player)) {
      server_->send(409, "application/json", "{\"ok\":false,\"error\":{\"code\":\"playerBusy\"}}");
      return;
    }
    server_->send(200, "application/json", "{\"ok\":true}");
  });
#endif
  server_->on("/display/sound/mute", HTTP_POST, [this]() {
    JsonDocument doc;
    if (!parse_body_(doc) || !doc.is<JsonObject>() || doc.as<JsonObject>().size() != 1 ||
        !doc["muted"].is<bool>()) {
      send_error_(param_error("muted boolean required")); return;
    }
    if (sound_mute_fn_ != nullptr) sound_mute_fn_(doc["muted"].as<bool>());
    server_->send(200, "application/json", "{\"ok\":true}");
  });
  // LED-L009 additive capability routes.
  server_->on("/display/timer/set", HTTP_POST, [this]() { handle_command_(SemanticCommand::Capability::TimerSet); });
  server_->on("/display/timer/start", HTTP_POST, [this]() { handle_command_(SemanticCommand::Capability::TimerStart); });
  server_->on("/display/timer/pause", HTTP_POST, [this]() { handle_command_(SemanticCommand::Capability::TimerPause); });
  server_->on("/display/timer/resume", HTTP_POST, [this]() { handle_command_(SemanticCommand::Capability::TimerResume); });
  server_->on("/display/timer/reset", HTTP_POST, [this]() { handle_command_(SemanticCommand::Capability::TimerReset); });
  server_->on("/display/timer/stop", HTTP_POST, [this]() { handle_command_(SemanticCommand::Capability::TimerStop); });
  server_->on("/display/input", HTTP_POST, [this]() { handle_command_(SemanticCommand::Capability::InputSend); });
  server_->on("/display/menu/set", HTTP_POST, [this]() { handle_command_(SemanticCommand::Capability::MenuSet); });
  server_->on("/display/game/set", HTTP_POST, [this]() { handle_command_(SemanticCommand::Capability::GameSet); });
  server_->on("/display/game/command", HTTP_POST, [this]() { handle_command_(SemanticCommand::Capability::GameCommand); });
  server_->on("/display/game/difficulty", HTTP_POST, [this]() { handle_command_(SemanticCommand::Capability::GameDifficulty); });
  server_->on("/display/visualizer/start", HTTP_POST, [this]() { handle_command_(SemanticCommand::Capability::VisualizerStart); });
  server_->on("/display/visualizer/stop", HTTP_POST, [this]() { handle_command_(SemanticCommand::Capability::VisualizerStop); });
  server_->on("/display/visualizer/data", HTTP_POST, [this]() { handle_command_(SemanticCommand::Capability::VisualizerData); });
  server_->on("/display/notify", HTTP_POST, [this]() { handle_command_(SemanticCommand::Capability::Notify); });
  server_->on("/display/standby/set", HTTP_POST, [this]() { handle_command_(SemanticCommand::Capability::StandbySet); });
  server_->on("/state", HTTP_GET, [this]() { send_state_(); });
  server_->on("/identity", HTTP_GET, [this]() { send_identity_(); });
  server_->on("/registry", HTTP_GET, [this]() { send_registry_(); });
  // LED-L004: same-origin dashboard at exactly GET / (specification:
  // ui::is_index_route). POST / and every other undocumented path still hit
  // the 404 handler below — accepted LED-L003 behavior is unchanged.
  server_->on("/", HTTP_GET, [this]() { serve_dashboard_(); });
  server_->onNotFound([this]() {
    // Unknown path: plain 404 — the typed vocabulary names capabilities, and a
    // random path carries no capability name.
    server_->send(404, "application/json",
                 "{\"ok\":false,\"error\":{\"code\":\"unsupportedCapability\"}}");
  });
  server_->begin();
  running_ = true;
}

void HttpApi::tick() {
  input_sessions_.expire(millis());
  server_->handleClient();
}

// Serves the embedded dashboard. Bounded static response (one PROGMEM string,
// sent by the WebServer's chunked writer) — no renderer interaction (§12).
void HttpApi::serve_dashboard_() {
#ifdef LED_WALL_ENABLE_DS4
  server_->sendHeader("Content-Encoding", "gzip");
  server_->send_P(200, ui::index_content_type(), ui::index_html(), ui::index_html_len());
#else
  server_->send_P(200, ui::index_content_type(), ui::index_html());
#endif
}

void HttpApi::handle_pixel_session_() {
  if (pixel_session_fn_ == nullptr) {
    server_->send(503, "application/json", "{\"ok\":false,\"error\":{\"code\":\"unsupportedCapability\"}}");
    return;
  }
  const String body = server_->hasArg("plain") ? server_->arg("plain") : String();
  if (body.length() > 0 && body != "{}") {
    server_->send(400, "application/json", "{\"ok\":false,\"error\":{\"code\":\"invalidParameter\"}}");
    return;
  }
  JsonDocument response;
  response["ok"] = true;
  response["session"] = pixel_session_fn_();
  response["revision"] = 0;
  String out;
  serializeJson(response, out);
  server_->send(200, "application/json", out);
}

void HttpApi::handle_pixel_frame_() {
  if (pixel_frame_fn_ == nullptr || !server_->hasArg("plain")) {
    server_->send(503, "application/json", "{\"ok\":false,\"error\":{\"code\":\"unsupportedCapability\"}}");
    return;
  }
  const String payload = server_->arg("plain");
  if (payload.length() < PixelCanvas::kHexChars || payload.length() > 3000u) {
    send_error_(param_error("pixel frame must be a bounded complete 450-cell snapshot"));
    return;
  }
  JsonDocument doc;
  if (deserializeJson(doc, payload) || !doc.is<JsonObject>()) {
    send_error_(param_error("pixel frame must be a JSON object"));
    return;
  }
  JsonObject obj = doc.as<JsonObject>();
  for (JsonPair entry : obj) {
    const char* key = entry.key().c_str();
    if (std::strcmp(key, "session") != 0 && std::strcmp(key, "revision") != 0 &&
        std::strcmp(key, "pixels") != 0 && std::strcmp(key, "live") != 0) {
      send_error_(param_error("unknown pixel frame parameter"));
      return;
    }
  }
  if (!obj["session"].is<uint32_t>() || !obj["revision"].is<uint32_t>() ||
      !obj["pixels"].is<const char*>() ||
      (!obj["live"].isNull() && !obj["live"].is<bool>())) {
    send_error_(param_error("session, revision and pixels are required"));
    return;
  }
  const char* pixels = obj["pixels"].as<const char*>();
  const auto result = pixel_frame_fn_(obj["session"].as<uint32_t>(),
                                       obj["revision"].as<uint32_t>(), pixels,
                                       std::strlen(pixels), obj["live"] | false, millis());
  if (result != PixelCanvas::ApplyResult::Accepted) {
    const char* message = result == PixelCanvas::ApplyResult::InvalidFrame ? "invalid complete pixel frame" :
                          result == PixelCanvas::ApplyResult::StaleSession ? "pixel session expired" :
                          result == PixelCanvas::ApplyResult::StaleRevision ? "stale pixel revision" :
                          result == PixelCanvas::ApplyResult::RateLimited ? "pixel live rate limit" :
                                                                           "Tail 150 is unavailable";
    CommandResponse error = param_error(message);
    if (result == PixelCanvas::ApplyResult::TailUnavailable) error.error = ErrorCode::LedUnavailable;
    send_error_(error);
    return;
  }
  JsonDocument response;
  response["ok"] = true;
  response["session"] = obj["session"].as<uint32_t>();
  response["revision"] = obj["revision"].as<uint32_t>();
  response["applied"] = true;
  String out;
  serializeJson(response, out);
  server_->send(200, "application/json", out);
}

void HttpApi::handle_pixel_projects_() {
  JsonDocument response;
  response["ok"] = true;
  response["capacity"] = 4;
  response["available"] = pixel_store_ready_;
  JsonArray names = response["projects"].to<JsonArray>();
#ifndef LED_WALL_ENABLE_DS4
  if (pixel_store_ready_) {
    File root = SPIFFS.open("/");
    for (File file = root.openNextFile(); file; file = root.openNextFile()) {
      String path = file.name();
      if (!path.startsWith("/px_") || path == "/px_pending.rgb") continue;
      if (path.endsWith(".bak") &&
          SPIFFS.exists(path.substring(0, path.length() - 4) + ".rgb")) continue;
      if (!path.endsWith(".rgb") && !path.endsWith(".bak")) continue;
      const String name = path.substring(4, path.length() - 4);
      if (pixel_project_name(name.c_str())) names.add(name);
    }
  }
#endif
  String out;
  serializeJson(response, out);
  server_->send(200, "application/json", out);
}

void HttpApi::handle_pixel_project_save_() {
  if (!pixel_store_ready_) {
    server_->send(503, "application/json", "{\"ok\":false,\"error\":{\"code\":\"storageUnavailable\"}}");
    return;
  }
  const String payload = server_->hasArg("plain") ? server_->arg("plain") : String();
  if (payload.length() < PixelCanvas::kHexChars || payload.length() > 3100u) {
    send_error_(param_error("bounded complete project required"));
    return;
  }
  JsonDocument doc;
  if (deserializeJson(doc, payload) || !doc.is<JsonObject>()) {
    send_error_(param_error("invalid project JSON"));
    return;
  }
  JsonObject obj = doc.as<JsonObject>();
  if (obj.size() != 3 || !obj["schemaVersion"].is<int>() ||
      obj["schemaVersion"].as<int>() != 1 || !obj["name"].is<const char*>() ||
      !obj["pixels"].is<const char*>()) {
    send_error_(param_error("project schemaVersion, name and pixels required"));
    return;
  }
  const char* name = obj["name"].as<const char*>();
  const char* pixels = obj["pixels"].as<const char*>();
  if (!pixel_project_name(name) || !pixel_hex(pixels)) {
    send_error_(param_error("invalid project name or complete frame"));
    return;
  }
#ifndef LED_WALL_ENABLE_DS4
  if (!pixel_exists(name) && pixel_project_count() >= 4) {
    server_->send(507, "application/json", "{\"ok\":false,\"error\":{\"code\":\"storageFull\"}}");
    return;
  }
  const String path = pixel_path(name);
  const String backup = pixel_backup(name);
  const String temp = String("/px_pending.rgb");
  SPIFFS.remove(temp);
  File file = SPIFFS.open(temp, FILE_WRITE);
  if (!file || file.write(reinterpret_cast<const uint8_t*>(pixels), PixelCanvas::kHexChars) !=
                   PixelCanvas::kHexChars) {
    if (file) file.close();
    SPIFFS.remove(temp);
    server_->send(507, "application/json", "{\"ok\":false,\"error\":{\"code\":\"storageFull\"}}");
    return;
  }
  file.close();
  if (SPIFFS.exists(backup)) SPIFFS.remove(backup);
  if (SPIFFS.exists(path) && !SPIFFS.rename(path, backup)) {
    SPIFFS.remove(temp);
    server_->send(500, "application/json", "{\"ok\":false,\"error\":{\"code\":\"storageError\"}}");
    return;
  }
  if (!SPIFFS.rename(temp, path)) {
    if (SPIFFS.exists(backup)) SPIFFS.rename(backup, path);
    SPIFFS.remove(temp);
    server_->send(500, "application/json", "{\"ok\":false,\"error\":{\"code\":\"storageError\"}}");
    return;
  }
  SPIFFS.remove(backup);
  JsonDocument response;
  response["ok"] = true;
  response["saved"] = name;
  String out;
  serializeJson(response, out);
  server_->send(200, "application/json", out);
#endif
}

void HttpApi::handle_pixel_project_load_() {
  if (!pixel_store_ready_) {
    server_->send(503, "application/json", "{\"ok\":false,\"error\":{\"code\":\"storageUnavailable\"}}");
    return;
  }
  const String payload = server_->hasArg("plain") ? server_->arg("plain") : String();
  if (payload.length() > 96u) { send_error_(param_error("project name too long")); return; }
  JsonDocument doc;
  if (deserializeJson(doc, payload) || !doc.is<JsonObject>() ||
      doc.as<JsonObject>().size() != 1 || !doc["name"].is<const char*>() ||
      !pixel_project_name(doc["name"].as<const char*>())) {
    send_error_(param_error("valid project name required"));
    return;
  }
#ifndef LED_WALL_ENABLE_DS4
  const char* name = doc["name"].as<const char*>();
  String path = pixel_path(name);
  if (!SPIFFS.exists(path)) path = pixel_backup(name);  // power-loss recovery
  File file = SPIFFS.open(path, FILE_READ);
  if (!file || file.size() != PixelCanvas::kHexChars) {
    if (file) file.close();
    server_->send(404, "application/json", "{\"ok\":false,\"error\":{\"code\":\"projectUnavailable\"}}");
    return;
  }
  String pixels;
  pixels.reserve(PixelCanvas::kHexChars + 1u);
  while (file.available()) pixels += static_cast<char>(file.read());
  file.close();
  if (!pixel_hex(pixels.c_str())) {
    server_->send(500, "application/json", "{\"ok\":false,\"error\":{\"code\":\"projectCorrupt\"}}");
    return;
  }
  JsonDocument response;
  response["ok"] = true;
  response["schemaVersion"] = 1;
  response["name"] = name;
  response["pixels"] = pixels;
  String out;
  serializeJson(response, out);
  server_->send(200, "application/json", out);
#endif
}

void HttpApi::handle_scene_play_() {
  if (scene_play_fn_ == nullptr) {
    server_->send(503, "application/json", "{\"ok\":false,\"error\":{\"code\":\"unsupportedCapability\"}}");
    return;
  }
  const String payload = server_->hasArg("plain") ? server_->arg("plain") : String();
  if (payload.length() == 0 || payload.length() > 1800u) {
    send_error_(param_error("bounded scene request required")); return;
  }
  JsonDocument doc;
  if (deserializeJson(doc, payload) || !doc.is<JsonObject>()) {
    send_error_(param_error("invalid scene JSON")); return;
  }
  JsonObject obj = doc.as<JsonObject>();
  SceneSpec spec;
  if (obj["preset"].is<const char*>()) {
    const bool game_night = std::strcmp(obj["preset"].as<const char*>(), "game_night") == 0;
    if (obj.size() > (game_night ? 2u : 1u) ||
        (obj.size() == 2u && !obj["game"].is<const char*>()) ||
        !scene_preset(obj["preset"].as<const char*>(), &spec)) {
      send_error_(param_error("unknown Party preset")); return;
    }
    if (game_night && !obj["game"].isNull()) {
      if (!obj["game"].is<const char*>() || !valid_game(obj["game"].as<const char*>())) {
        send_error_(param_error("Game Night requires a registered game")); return;
      }
      std::strncpy(spec.steps[2].content, obj["game"].as<const char*>(),
                   sizeof(spec.steps[2].content) - 1);
    }
  } else {
    if (!parse_custom_scene(obj, &spec)) {
      send_error_(param_error("invalid bounded scene recipe")); return;
    }
  }
  if (!scene_play_fn_(spec, millis())) {
    server_->send(409, "application/json", "{\"ok\":false,\"error\":{\"code\":\"sceneUnavailable\"}}");
    return;
  }
  JsonDocument response;
  response["ok"] = true;
  response["playing"] = spec.name;
  String out;
  serializeJson(response, out);
  server_->send(200, "application/json", out);
}

void HttpApi::handle_scene_projects_() {
  JsonDocument response;
  response["ok"] = true;
  response["capacity"] = 4;
  response["available"] = pixel_store_ready_;
  JsonArray names = response["projects"].to<JsonArray>();
#ifndef LED_WALL_ENABLE_DS4
  if (pixel_store_ready_) {
    File root = SPIFFS.open("/");
    for (File file = root.openNextFile(); file; file = root.openNextFile()) {
      const String path = file.name();
      if (!path.startsWith("/sc_") || path == "/sc_pending.json") continue;
      if (path.endsWith(".bak") &&
          SPIFFS.exists(path.substring(0, path.length() - 4) + ".json")) continue;
      if (!path.endsWith(".json") && !path.endsWith(".bak")) continue;
      const String name = path.substring(4, path.length() -
                                             (path.endsWith(".json") ? 5 : 4));
      if (pixel_project_name(name.c_str())) names.add(name);
    }
  }
#endif
  String out;
  serializeJson(response, out);
  server_->send(200, "application/json", out);
}

void HttpApi::handle_scene_project_save_() {
  if (!pixel_store_ready_) {
    server_->send(503, "application/json", "{\"ok\":false,\"error\":{\"code\":\"storageUnavailable\"}}");
    return;
  }
  const String payload = server_->hasArg("plain") ? server_->arg("plain") : String();
  if (payload.length() == 0 || payload.length() > 1900u) {
    send_error_(param_error("bounded scene project required")); return;
  }
  JsonDocument doc;
  if (deserializeJson(doc, payload) || !doc.is<JsonObject>() ||
      doc.as<JsonObject>().size() != 2 || !doc["schemaVersion"].is<int>() ||
      doc["schemaVersion"].as<int>() != 1 || !doc["scene"].is<JsonObject>()) {
    send_error_(param_error("scene project schemaVersion and scene required")); return;
  }
  SceneSpec spec;
  if (!parse_custom_scene(doc["scene"].as<JsonObject>(), &spec)) {
    send_error_(param_error("invalid scene project")); return;
  }
#ifndef LED_WALL_ENABLE_DS4
  if (!SPIFFS.exists(scene_path(spec.name)) && !SPIFFS.exists(scene_backup(spec.name)) &&
      scene_project_count() >= 4) {
    server_->send(507, "application/json", "{\"ok\":false,\"error\":{\"code\":\"storageFull\"}}");
    return;
  }
  String body;
  serializeJson(doc["scene"], body);
  const String path = scene_path(spec.name), backup = scene_backup(spec.name);
  const String temp = "/sc_pending.json";
  SPIFFS.remove(temp);
  File file = SPIFFS.open(temp, FILE_WRITE);
  if (!file || file.write(reinterpret_cast<const uint8_t*>(body.c_str()), body.length()) !=
                   body.length()) {
    if (file) file.close();
    SPIFFS.remove(temp);
    server_->send(507, "application/json", "{\"ok\":false,\"error\":{\"code\":\"storageFull\"}}");
    return;
  }
  file.close();
  if (SPIFFS.exists(backup)) SPIFFS.remove(backup);
  if (SPIFFS.exists(path) && !SPIFFS.rename(path, backup)) {
    SPIFFS.remove(temp);
    server_->send(500, "application/json", "{\"ok\":false,\"error\":{\"code\":\"storageError\"}}");
    return;
  }
  if (!SPIFFS.rename(temp, path)) {
    if (SPIFFS.exists(backup)) SPIFFS.rename(backup, path);
    SPIFFS.remove(temp);
    server_->send(500, "application/json", "{\"ok\":false,\"error\":{\"code\":\"storageError\"}}");
    return;
  }
  SPIFFS.remove(backup);
  JsonDocument response;
  response["ok"] = true;
  response["saved"] = spec.name;
  String out;
  serializeJson(response, out);
  server_->send(200, "application/json", out);
#endif
}

void HttpApi::handle_scene_project_load_() {
  if (!pixel_store_ready_) {
    server_->send(503, "application/json", "{\"ok\":false,\"error\":{\"code\":\"storageUnavailable\"}}");
    return;
  }
  const String payload = server_->hasArg("plain") ? server_->arg("plain") : String();
  if (payload.length() > 96u) { send_error_(param_error("scene name too long")); return; }
  JsonDocument request;
  if (deserializeJson(request, payload) || !request.is<JsonObject>() ||
      request.as<JsonObject>().size() != 1 || !request["name"].is<const char*>() ||
      !pixel_project_name(request["name"].as<const char*>())) {
    send_error_(param_error("valid scene name required")); return;
  }
#ifndef LED_WALL_ENABLE_DS4
  const char* name = request["name"].as<const char*>();
  String path = scene_path(name);
  if (!SPIFFS.exists(path)) path = scene_backup(name);
  File file = SPIFFS.open(path, FILE_READ);
  if (!file || file.size() > 1800u) {
    if (file) file.close();
    server_->send(404, "application/json", "{\"ok\":false,\"error\":{\"code\":\"projectUnavailable\"}}");
    return;
  }
  String body;
  body.reserve(file.size() + 1u);
  while (file.available()) body += static_cast<char>(file.read());
  file.close();
  JsonDocument scene;
  SceneSpec spec;
  if (deserializeJson(scene, body) || !scene.is<JsonObject>() ||
      !parse_custom_scene(scene.as<JsonObject>(), &spec) || std::strcmp(spec.name, name) != 0) {
    server_->send(500, "application/json", "{\"ok\":false,\"error\":{\"code\":\"projectCorrupt\"}}");
    return;
  }
  JsonDocument response;
  response["ok"] = true;
  response["schemaVersion"] = 1;
  response["scene"] = scene.as<JsonObject>();
  String out;
  serializeJson(response, out);
  server_->send(200, "application/json", out);
#endif
}

void HttpApi::handle_input_session_() {
  JsonDocument doc;
  if (!parse_body_(doc) || !doc.is<JsonObject>() || doc.as<JsonObject>().size() != 1 ||
      !doc["player"].is<int>() || doc["player"].as<int>() < 1 ||
      doc["player"].as<int>() > 2) {
    send_error_(param_error("player 1 or 2 required")); return;
  }
  const uint8_t player = static_cast<uint8_t>(doc["player"].as<int>());
#ifdef LED_WALL_ENABLE_DS4
  if (ds4::player_claimed(player)) {
    server_->send(409, "application/json", "{\"ok\":false,\"error\":{\"code\":\"playerBusy\"}}");
    return;
  }
#endif
  uint32_t token = esp_random();
  if (token == 0) token = 1;
  const auto result = input_sessions_.claim(player, token, millis());
  if (result != InputSessions::Result::Accepted) {
    server_->send(409, "application/json", "{\"ok\":false,\"error\":{\"code\":\"playerBusy\"}}");
    return;
  }
  JsonDocument response;
  response["ok"] = true;
  response["player"] = player;
  response["session"] = token;
  response["sequence"] = 0;
  String out;
  serializeJson(response, out);
  server_->send(200, "application/json", out);
}

void HttpApi::handle_input_event_() {
  JsonDocument doc;
  if (!parse_body_(doc) || !doc.is<JsonObject>() || doc.as<JsonObject>().size() != 5 ||
      !doc["player"].is<int>() || doc["player"].as<int>() < 1 || doc["player"].as<int>() > 2 ||
      !doc["session"].is<uint32_t>() || !doc["sequence"].is<uint32_t>() ||
      !doc["input"].is<const char*>() || !doc["state"].is<const char*>()) {
    send_error_(param_error("player, session, sequence, input and state required")); return;
  }
  const char* state = doc["state"].as<const char*>();
  if (std::strcmp(state, "press") != 0 && std::strcmp(state, "release") != 0) {
    send_error_(param_error("state must be press or release")); return;
  }
  InputEvent input;
  if (!input_event_from_name(doc["input"].as<const char*>(), &input)) {
    send_error_(param_error("unknown input")); return;
  }
  const auto result = input_sessions_.event(
      static_cast<uint8_t>(doc["player"].as<int>()), doc["session"].as<uint32_t>(),
      doc["sequence"].as<uint32_t>(), input, std::strcmp(state, "press") == 0,
      millis(), input_queue_);
  if (result != InputSessions::Result::Accepted && result != InputSessions::Result::Duplicate) {
    server_->send(409, "application/json", "{\"ok\":false,\"error\":{\"code\":\"inputSessionRejected\"}}");
    return;
  }
  JsonDocument response;
  response["ok"] = true;
  response["queued"] = result == InputSessions::Result::Accepted && std::strcmp(state, "press") == 0;
  response["sequence"] = doc["sequence"].as<uint32_t>();
  String out;
  serializeJson(response, out);
  server_->send(200, "application/json", out);
}

void HttpApi::handle_input_heartbeat_() {
  JsonDocument doc;
  if (!parse_body_(doc) || !doc.is<JsonObject>() || doc.as<JsonObject>().size() != 3 ||
      !doc["player"].is<int>() || doc["player"].as<int>() < 1 || doc["player"].as<int>() > 2 ||
      !doc["session"].is<uint32_t>() || !doc["sequence"].is<uint32_t>()) {
    send_error_(param_error("player, session and sequence required")); return;
  }
  const auto result = input_sessions_.heartbeat(
      static_cast<uint8_t>(doc["player"].as<int>()), doc["session"].as<uint32_t>(),
      doc["sequence"].as<uint32_t>(), millis());
  if (result != InputSessions::Result::Accepted) {
    server_->send(409, "application/json", "{\"ok\":false,\"error\":{\"code\":\"inputSessionRejected\"}}");
    return;
  }
  JsonDocument response;
  response["ok"] = true;
  response["sequence"] = doc["sequence"].as<uint32_t>();
  String out;
  serializeJson(response, out);
  server_->send(200, "application/json", out);
}

void HttpApi::handle_input_release_() {
  JsonDocument doc;
  if (!parse_body_(doc) || !doc.is<JsonObject>() || doc.as<JsonObject>().size() != 2 ||
      !doc["player"].is<int>() || doc["player"].as<int>() < 1 || doc["player"].as<int>() > 2 ||
      !doc["session"].is<uint32_t>()) {
    send_error_(param_error("player and session required")); return;
  }
  const auto result = input_sessions_.release(static_cast<uint8_t>(doc["player"].as<int>()),
                                              doc["session"].as<uint32_t>());
  if (result != InputSessions::Result::Accepted) {
    server_->send(409, "application/json", "{\"ok\":false,\"error\":{\"code\":\"inputSessionRejected\"}}");
    return;
  }
  server_->send(200, "application/json", "{\"ok\":true,\"released\":true}");
}

bool HttpApi::parse_body_(JsonDocument& doc) {
  if (!server_->hasArg("plain")) {
    return true;  // absent/empty body → empty-object semantics
  }
  const String body = server_->arg("plain");
  if (body.length() == 0) {
    return true;
  }
  if (body.length() > 1024) {
    return false;  // oversized body (largest legal command: 30-band visualizer feed)
  }
  const DeserializationError err = deserializeJson(doc, body);
  return !err;
}

void HttpApi::send_ok_(const CommandResponse& response) {
  JsonDocument doc;
  doc["ok"] = true;
  // LED-L007: display.text.show acks report the §6 skippedCharacters count
  // (unsupported characters rendered as space).
  if (response.has_skipped_characters) {
    doc["skippedCharacters"] = response.skipped_characters;
  }
  String out;
  serializeJson(doc, out);
  server_->send(200, "application/json", out);
}

void HttpApi::send_error_(const CommandResponse& response) {
  JsonDocument doc;
  serialize_error(response.error, response.message, response.available, doc);
  String out;
  serializeJson(doc, out);
  server_->send(400, "application/json", out);
}

void HttpApi::send_state_() {
  JsonDocument doc;
  serialize_state(state_fn_(), identity_fn_(), doc);
  String out;
  serializeJson(doc, out);
  server_->send(200, "application/json", out);
}

void HttpApi::send_identity_() {
  JsonDocument doc;
  serialize_identity(identity_fn_(), doc);
  String out;
  serializeJson(doc, out);
  server_->send(200, "application/json", out);
}

// LED-L009 registry discovery: the current registries so premium clients and
// future adapter versions can enumerate without embedding vocabularies.
void HttpApi::send_registry_() {
  JsonDocument doc;
  // Consumer discovery is the coordinated V5 catalog. The historical
  // animation IDs remain below for existing semantic API clients.
  JsonArray featured = doc["publicScenes"].to<JsonArray>();
  static constexpr const char* kPublicScenes[] = {
      "fireworks", "meteor_impact", "energy_bloom", "rainbow_river", "storm",
      "lava_fountain", "rain_drain", "aurora_current", "starstream", "heartbeat"};
  for (const char* scene : kPublicScenes) featured.add(scene);
  JsonArray moments = doc["partyMoments"].to<JsonArray>();
  static constexpr const char* kPartyMoments[] = {
      "welcome", "birthday", "celebration", "game_night", "calm", "countdown_finale"};
  for (const char* moment : kPartyMoments) moments.add(moment);
  JsonArray anims = doc["animations"].to<JsonArray>();
  for (uint8_t i = 0; i < kAnimationCount; ++i) {
    anims.add(kAnimationNames[i]);
  }
  JsonArray exprs = doc["expressions"].to<JsonArray>();
  for (uint8_t i = 0; i < kExpressionCount; ++i) {
    exprs.add(kExpressionNames[i]);
  }
  JsonArray games = doc["games"].to<JsonArray>();
  for (uint8_t i = 0; i < kGameCount; ++i) {
    games.add(kGameNames[i]);
  }
  JsonArray inputs = doc["inputs"].to<JsonArray>();
  for (uint8_t i = 0; i < kInputEventCount; ++i) {
    inputs.add(kInputEventNames[i]);
  }
  JsonArray styles = doc["textStyles"].to<JsonArray>();
  for (uint8_t i = 0; i < text::kTextStyleCount; ++i) {
    styles.add(text::kTextStyleNames[i]);
  }
  JsonArray palette = doc["palette"].to<JsonArray>();
  for (uint8_t i = 0; i < text::kTextPaletteCount; ++i) {
    palette.add(text::kTextPalette[i].name);
  }
  // LED-L010 zone + player metadata (capability discovery — the accepted
  // adapters keep their valid subset; newer clients discover here).
  JsonObject zones = doc["animationZones"].to<JsonObject>();
  for (uint8_t i = 0; i < kAnimationCount; ++i) {
    const uint8_t mask = animation_zone_mask(kAnimationNames[i]);
    JsonArray supported = zones[kAnimationNames[i]].to<JsonArray>();
    if (mask & zone_flags::kMatrix) {
      supported.add("matrix");
    }
    if (mask & zone_flags::kTail) {
      supported.add("tail");
    }
    if (mask & zone_flags::kBoth) {
      supported.add("both");
    }
  }
  JsonObject gameMeta = doc["gameMeta"].to<JsonObject>();
  for (uint8_t i = 0; i < kGameCount; ++i) {
    JsonObject meta = gameMeta[kGameNames[i]].to<JsonObject>();
    meta["zone"] = zone_target_name(game_zone(kGameNames[i]));
    meta["players"] = game_max_players(kGameNames[i]);
    meta["minPlayers"] = game_min_players(kGameNames[i]);
    meta["minLane"] = game_min_lane(kGameNames[i]);
  }
  String out;
  serializeJson(doc, out);
  server_->send(200, "application/json", out);
}

// Translates the HTTP request into a pure SemanticCommand and executes it.
// Strict param checking: unknown fields are rejected (§11 malformed commands
// mutate nothing). The dispatcher owns all semantic validation.
void HttpApi::handle_command_(SemanticCommand::Capability capability) {
  JsonDocument body;
  if (!parse_body_(body)) {
    send_error_(param_error("request body must be a JSON object within the size limit"));
    return;
  }
  for (JsonPair kv : body.as<JsonObject>()) {
    if (capability == SemanticCommand::Capability::TailPoint ||
        capability == SemanticCommand::Capability::TailPointOff) {
      if (capability == SemanticCommand::Capability::TailPoint && kv.key() == "index") continue;
      send_error_(param_error("Tail Setup accepts only an index for point, or an empty OFF command"));
      return;
    }
    const bool known =
        (kv.key() == "text") || (kv.key() == "scroll") || (kv.key() == "speedMsPerPx") ||
        (kv.key() == "durationMs") || (kv.key() == "scene") || (kv.key() == "animation") ||
        (kv.key() == "expression") || (kv.key() == "level") ||
        // LED-L009 parameters.
        (kv.key() == "mode") || (kv.key() == "direction") || (kv.key() == "loop") ||
        (kv.key() == "wordDwellMs") || (kv.key() == "style") || (kv.key() == "color") ||
        (kv.key() == "bgColor") || (kv.key() == "action") || (kv.key() == "input") ||
        (kv.key() == "game") || (kv.key() == "bands") || (kv.key() == "priority") ||
        (kv.key() == "on") || (kv.key() == "difficulty") ||
        // LED-L010 parameters.
        (kv.key() == "target") || (kv.key() == "player") || (kv.key() == "players") ||
        (kv.key() == "tailMode") || (kv.key() == "tailColor");
    if (!known) {
      send_error_(param_error("unknown parameter"));
      return;
    }
  }

  SemanticCommand cmd;
  cmd.capability = capability;

  switch (capability) {
    case SemanticCommand::Capability::TailPoint: {
      JsonVariant v = body["index"];
      if (!v.is<int>() || v.as<int>() < 0 || v.as<int>() >= 200) {
        send_error_(param_error("index (integer 0..199) is required"));
        return;
      }
      cmd.has_tail_index = true;
      cmd.tail_index = static_cast<uint16_t>(v.as<int>());
      break;
    }
    case SemanticCommand::Capability::TailPointOff:
      break;
    case SemanticCommand::Capability::TextShow: {
      JsonVariant v = body["text"];
      if (!v.is<const char*>()) {
        send_error_(param_error("text (string) is required"));
        return;
      }
      const char* text = v.as<const char*>();
      if (std::strlen(text) > kTextProtocolCap) {
        send_error_(param_error("text exceeds the 128 character protocol cap"));
        return;
      }
      std::strncpy(cmd.text, text, kTextMaxLen - 1);
      cmd.text[kTextMaxLen - 1] = '\0';
      if (body["scroll"].is<bool>()) {
        cmd.has_scroll = true;
        cmd.scroll = body["scroll"].as<bool>();
      }
      if (!body["speedMsPerPx"].isNull()) {
        if (!body["speedMsPerPx"].is<uint32_t>()) {
          send_error_(param_error("speedMsPerPx must be a positive integer"));
          return;
        }
        cmd.has_speed = true;
        cmd.speed_ms_per_px = body["speedMsPerPx"].as<uint32_t>();
      }
      // LED-L009 Text Studio parameters (all optional, strictly typed).
      if (!body["mode"].isNull()) {
        if (!body["mode"].is<const char*>() ||
            !text_mode_from_name(body["mode"].as<const char*>(), &cmd.mode)) {
          send_error_(param_error("mode must be fixed, marquee or words"));
          return;
        }
        cmd.has_mode = true;
      }
      if (!body["direction"].isNull()) {
        if (!body["direction"].is<const char*>()) {
          send_error_(param_error("direction must be a string (rtl|ltr)"));
          return;
        }
        const char* dir = body["direction"].as<const char*>();
        if (std::strcmp(dir, "rtl") == 0) {
          cmd.rtl = true;
        } else if (std::strcmp(dir, "ltr") == 0) {
          cmd.rtl = false;
        } else {
          send_error_(param_error("direction must be rtl or ltr"));
          return;
        }
        cmd.has_direction = true;
      }
      if (!body["loop"].isNull()) {
        if (!body["loop"].is<bool>()) {
          send_error_(param_error("loop must be a boolean"));
          return;
        }
        cmd.has_loop = true;
        cmd.loop = body["loop"].as<bool>();
      }
      if (!body["wordDwellMs"].isNull()) {
        if (!body["wordDwellMs"].is<uint32_t>()) {
          send_error_(param_error("wordDwellMs must be a positive integer"));
          return;
        }
        cmd.has_word_dwell = true;
        cmd.word_dwell_ms = body["wordDwellMs"].as<uint32_t>();
      }
      if (!body["style"].isNull()) {
        if (!body["style"].is<const char*>() ||
            !text::text_style_from_name(body["style"].as<const char*>(), &cmd.style)) {
          send_error_(param_error("style must be one of the semantic text styles"));
          return;
        }
        cmd.has_style = true;
      }
      if (!body["color"].isNull()) {
        if (!body["color"].is<const char*>() ||
            !text::text_palette_index_from_name(body["color"].as<const char*>(), &cmd.fg_color)) {
          send_error_(param_error("color must be one of the semantic palette names"));
          return;
        }
        cmd.has_fg_color = true;
      }
      if (!body["bgColor"].isNull()) {
        if (!body["bgColor"].is<const char*>() ||
            !text::text_palette_index_from_name(body["bgColor"].as<const char*>(), &cmd.bg_color)) {
          send_error_(param_error("bgColor must be one of the semantic palette names"));
          return;
        }
        cmd.has_bg_color = true;
      }
      if (!body["tailMode"].isNull()) {
        if (!body["tailMode"].is<const char*>() ||
            !text_tail_mode_from_name(body["tailMode"].as<const char*>(), &cmd.tail_mode)) {
          send_error_(param_error("tailMode must be a supported Text Tail mode"));
          return;
        }
        cmd.has_tail_mode = true;
      }
      if (!body["tailColor"].isNull()) {
        if (!body["tailColor"].is<const char*>() ||
            !text::text_palette_index_from_name(body["tailColor"].as<const char*>(), &cmd.tail_color)) {
          send_error_(param_error("tailColor must be a semantic palette name"));
          return;
        }
        cmd.has_tail_color = true;
      }
      break;
    }
    case SemanticCommand::Capability::SceneSet:
    case SemanticCommand::Capability::AnimationSet:
    case SemanticCommand::Capability::ExpressionSet: {
      const char* param = (capability == SemanticCommand::Capability::SceneSet) ? "scene"
                          : (capability == SemanticCommand::Capability::AnimationSet) ? "animation"
                                                                                      : "expression";
      JsonVariant v = body[param];
      if (!v.is<const char*>()) {
        send_error_(param_error("program name (string) is required"));
        return;
      }
      std::strncpy(cmd.name, v.as<const char*>(), kMaxProgramNameLen - 1);
      cmd.name[kMaxProgramNameLen - 1] = '\0';
      // LED-L010: optional zone target. Repair cycle 1 (review minor): the
      // target field is parsed for ALL three program kinds — an expression
      // carrying "target" now reaches the dispatcher, which rejects it with a
      // typed invalidParameter (strict-parse heritage §17) instead of the
      // field being silently dropped here.
      if (!body["target"].isNull()) {
        if (!body["target"].is<const char*>()) {
          send_error_(param_error("target must be a string (matrix|tail|both)"));
          return;
        }
        std::strncpy(cmd.target, body["target"].as<const char*>(), sizeof(cmd.target) - 1);
        cmd.target[sizeof(cmd.target) - 1] = '\0';
        cmd.has_target = true;
      }
      break;
    }
    case SemanticCommand::Capability::BrightnessSet: {
      JsonVariant v = body["level"];
      if (v.isNull() || !v.is<uint32_t>()) {
        send_error_(param_error("level (integer 0..100) is required"));
        return;
      }
      cmd.has_level = true;
      cmd.level = v.as<uint32_t>();
      break;
    }
    // ------------------------------------------------ LED-L009 parsing ----
    case SemanticCommand::Capability::TimerSet: {
      JsonVariant v = body["durationMs"];
      if (v.isNull() || !v.is<uint32_t>()) {
        send_error_(param_error("durationMs (positive integer) is required"));
        return;
      }
      cmd.has_duration = true;
      cmd.duration_ms = v.as<uint32_t>();
      break;
    }
    case SemanticCommand::Capability::InputSend: {
      JsonVariant v = body["input"];
      if (!v.is<const char*>()) {
        send_error_(param_error("input (string) is required"));
        return;
      }
      std::strncpy(cmd.input, v.as<const char*>(), sizeof(cmd.input) - 1);
      cmd.input[sizeof(cmd.input) - 1] = '\0';
      cmd.has_input = true;
      // LED-L010: optional player slot (1|2; absent = P1).
      if (!body["player"].isNull()) {
        if (!body["player"].is<int>() || body["player"].as<int>() < 1 ||
            body["player"].as<int>() > 2) {
          send_error_(param_error("player must be 1 or 2"));
          return;
        }
        cmd.has_player = true;
        cmd.player = static_cast<uint8_t>(body["player"].as<int>());
      }
      break;
    }
    case SemanticCommand::Capability::MenuSet:
    case SemanticCommand::Capability::GameCommand: {
      JsonVariant v = body["action"];
      if (!v.is<const char*>()) {
        send_error_(param_error("action (string) is required"));
        return;
      }
      std::strncpy(cmd.action, v.as<const char*>(), sizeof(cmd.action) - 1);
      cmd.action[sizeof(cmd.action) - 1] = '\0';
      cmd.has_action = true;
      break;
    }
    case SemanticCommand::Capability::GameSet: {
      JsonVariant v = body["game"];
      if (!v.is<const char*>()) {
        send_error_(param_error("game (string) is required"));
        return;
      }
      std::strncpy(cmd.name, v.as<const char*>(), kMaxProgramNameLen - 1);
      cmd.name[kMaxProgramNameLen - 1] = '\0';
      // LED-L010: optional player mode (1|2; validated against the game's
      // registry metadata in the dispatcher).
      if (!body["players"].isNull()) {
        if (!body["players"].is<int>() || body["players"].as<int>() < 1 ||
            body["players"].as<int>() > 2) {
          send_error_(param_error("players must be 1 or 2"));
          return;
        }
        cmd.has_players = true;
        cmd.players = static_cast<uint8_t>(body["players"].as<int>());
      }
      break;
    }
    case SemanticCommand::Capability::GameDifficulty: {
      JsonVariant v = body["difficulty"];
      if (!v.is<const char*>()) {
        send_error_(param_error("difficulty (string) is required"));
        return;
      }
      std::strncpy(cmd.difficulty, v.as<const char*>(), sizeof(cmd.difficulty) - 1);
      cmd.difficulty[sizeof(cmd.difficulty) - 1] = '\0';
      cmd.has_difficulty = true;
      break;
    }
    case SemanticCommand::Capability::VisualizerData: {
      JsonArray arr = body["bands"].as<JsonArray>();
      if (arr.isNull() || arr.size() != kSpectrumBands) {
        send_error_(param_error("bands (array of 30 integers 0..255) is required"));
        return;
      }
      uint8_t i = 0;
      for (JsonVariant b : arr) {
        // Non-integer or out-of-range entries reject the whole feed.
        if (!b.is<int>() || b.as<int>() < 0 || b.as<int>() > 255) {
          send_error_(param_error("bands (array of 30 integers 0..255) is required"));
          return;
        }
        cmd.bands[i++] = static_cast<uint8_t>(b.as<int>());
      }
      break;
    }
    case SemanticCommand::Capability::Notify: {
      JsonVariant v = body["text"];
      if (!v.is<const char*>()) {
        send_error_(param_error("text (string) is required"));
        return;
      }
      const char* text = v.as<const char*>();
      if (std::strlen(text) > kTextProtocolCap) {
        send_error_(param_error("text exceeds the 128 character protocol cap"));
        return;
      }
      std::strncpy(cmd.text, text, kTextMaxLen - 1);
      cmd.text[kTextMaxLen - 1] = '\0';
      JsonVariant pri = body["priority"];
      if (!pri.isNull()) {
        if (!pri.is<const char*>()) {
          send_error_(param_error("priority must be a string (normal|high)"));
          return;
        }
        std::strncpy(cmd.priority, pri.as<const char*>(), sizeof(cmd.priority) - 1);
        cmd.priority[sizeof(cmd.priority) - 1] = '\0';
        cmd.has_priority = true;
      }
      break;
    }
    case SemanticCommand::Capability::StandbySet: {
      JsonVariant v = body["on"];
      if (!v.is<bool>()) {
        send_error_(param_error("on (boolean) is required"));
        return;
      }
      cmd.has_on = true;
      cmd.on = v.as<bool>();
      break;
    }
    case SemanticCommand::Capability::TextClear:
    case SemanticCommand::Capability::Clear:
    case SemanticCommand::Capability::StateRead:
    case SemanticCommand::Capability::TimerStart:
    case SemanticCommand::Capability::TimerPause:
    case SemanticCommand::Capability::TimerResume:
    case SemanticCommand::Capability::TimerReset:
    case SemanticCommand::Capability::TimerStop:
    case SemanticCommand::Capability::VisualizerStart:
    case SemanticCommand::Capability::VisualizerStop:
      break;  // no parameters
  }

  if (!body["durationMs"].isNull() &&
      (capability == SemanticCommand::Capability::TextShow ||
       capability == SemanticCommand::Capability::SceneSet ||
       capability == SemanticCommand::Capability::AnimationSet ||
       capability == SemanticCommand::Capability::Notify)) {
    if (!body["durationMs"].is<uint32_t>()) {
      send_error_(param_error("durationMs must be a positive integer"));
      return;
    }
    cmd.has_duration = true;
    cmd.duration_ms = body["durationMs"].as<uint32_t>();
  }

  if (capability == SemanticCommand::Capability::InputSend &&
      input_sessions_.claimed(cmd.has_player ? cmd.player : 1)) {
    server_->send(409, "application/json", "{\"ok\":false,\"error\":{\"code\":\"playerBusy\"}}");
    return;
  }
  const CommandResponse response = dispatcher_->execute(cmd, millis());
  if (response.ok) {
    send_ok_(response);
  } else {
    send_error_(response);
  }
}

}  // namespace ledwall
