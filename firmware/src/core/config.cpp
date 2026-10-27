#include "core/config.h"

#include <cstdio>
#include <cstring>

namespace ledwall {

namespace {

constexpr size_t kWifiSsidMax = 33;    // 32 chars + NUL
constexpr size_t kWifiPasswordMax = 65;  // 64 chars + NUL

bool parse_bool(const char* value, bool* out) {
  if (value == nullptr) {
    return false;
  }
  if (std::strcmp(value, "0") == 0) {
    *out = false;
    return true;
  }
  if (std::strcmp(value, "1") == 0) {
    *out = true;
    return true;
  }
  return false;
}

bool parse_u32(const char* value, uint32_t* out) {
  if (value == nullptr || *value == '\0') {
    return false;
  }
  uint32_t acc = 0;
  for (const char* p = value; *p != '\0'; ++p) {
    if (*p < '0' || *p > '9') {
      return false;
    }
    const uint32_t digit = static_cast<uint32_t>(*p - '0');
    if (acc > (0xFFFFFFFFu - digit) / 10u) {
      return false;  // overflow → invalid
    }
    acc = acc * 10u + digit;
  }
  *out = acc;
  return true;
}

}  // namespace

ConfigEntryResult apply_config_entry(AppConfig& config, const char* key, const char* value) {
  ConfigEntryResult result;
  const char* rejected = "ignored config key '%s': invalid value '%s' (safe fallback)";
  const char* unknown = "ignored unknown config key '%s'";

  auto reject = [&](char* dst, const char* fmt, const char* k, const char* v) {
    std::snprintf(dst, kMaxConfigWarningLen, fmt, k, v);
  };

  if (std::strcmp(key, "orientation.flip_odd_rows") == 0) {
    bool v = false;
    if (parse_bool(value, &v)) {
      config.orientation.flip_odd_rows = v;
      result.applied = true;
    } else {
      reject(result.warning, rejected, key, value);
    }
    return result;
  }
  if (std::strcmp(key, "orientation.flip_y") == 0) {
    bool v = false;
    if (parse_bool(value, &v)) {
      config.orientation.flip_y = v;
      result.applied = true;
    } else {
      reject(result.warning, rejected, key, value);
    }
    return result;
  }
  if (std::strcmp(key, "brightness.default_level") == 0) {
    uint32_t v = 0;
    if (parse_u32(value, &v) && v <= Brightness::kMaxLevel) {
      config.brightness_default_level = v;
      result.applied = true;
    } else {
      reject(result.warning, rejected, key, value);
    }
    return result;
  }
  if (std::strcmp(key, "brightness.ceiling") == 0) {
    uint32_t v = 0;
    if (parse_u32(value, &v) && v <= Brightness::kMaxLevel) {
      config.brightness_ceiling = v;
      result.applied = true;
    } else {
      reject(result.warning, rejected, key, value);
    }
    return result;
  }
  if (std::strcmp(key, "boot.base_mode") == 0) {
    ModeKind v = ModeKind::Idle;
    if (mode_from_name(value, &v)) {
      config.boot_base_mode = v;
      result.applied = true;
    } else {
      reject(result.warning, rejected, key, value);
    }
    return result;
  }
  if (std::strcmp(key, "wifi.ssid") == 0) {
    // Empty string clears the stored SSID (a deliberate, expressible state).
    if (value != nullptr && std::strlen(value) < kWifiSsidMax) {
      std::strncpy(config.wifi_ssid, value, kWifiSsidMax);
      result.applied = true;
    } else {
      reject(result.warning, "ignored config key 'wifi.ssid': value too long", key, value);
    }
    return result;
  }
  if (std::strcmp(key, "wifi.password") == 0) {
    if (value != nullptr && std::strlen(value) < kWifiPasswordMax) {
      std::strncpy(config.wifi_password, value, kWifiPasswordMax);
      result.applied = true;
    } else {
      reject(result.warning, "ignored config key 'wifi.password': value too long", key, value);
    }
    return result;
  }
  if (std::strcmp(key, "api.http_port") == 0 || std::strcmp(key, "api.ws_port") == 0) {
    uint32_t v = 0;
    if (parse_u32(value, &v) && v >= 1 && v <= 65535) {
      if (key[4] == 'h') {
        config.http_port = v;
      } else {
        config.ws_port = v;
      }
      result.applied = true;
    } else {
      reject(result.warning, "ignored config key '%s': invalid value '%s' (port must be 1..65535)",
             key, value);
    }
    return result;
  }
  if (std::strcmp(key, "input.enabled") == 0) {
    // LED-L009 physical input provider. Ships OFF: the joystick shield pinout
    // is not evidenced in the repository (input_pins.h); enabling polls the
    // candidate pins and must only be done after the Owner confirms wiring.
    bool v = false;
    if (parse_bool(value, &v)) {
      config.input_enabled = v;
      result.applied = true;
    } else {
      reject(result.warning, rejected, key, value);
    }
    return result;
  }
  if (std::strcmp(key, "tail.enabled") == 0) {
    // LED-L010 tail zone. Ships OFF until the Owner wires the rope (the pin
    // handoff in the LED-L010 evidence record precedes any physical work).
    bool v = false;
    if (parse_bool(value, &v)) {
      config.tail.enabled = v;
      result.applied = true;
    } else {
      reject(result.warning, rejected, key, value);
    }
    return result;
  }
  if (std::strcmp(key, "tail.length") == 0) {
    // Physical LED count of the tail rope — a hardware fact (never guessed).
    uint32_t v = 0;
    if (parse_u32(value, &v) && v >= 1 && v <= TailFramebuffer::kMaxLength) {
      config.tail.length = static_cast<uint16_t>(v);
      result.applied = true;
    } else {
      reject(result.warning,
             "ignored config key '%s': invalid value '%s' (length must be 1..300)", key, value);
    }
    return result;
  }
  if (std::strcmp(key, "tail.reverse") == 0) {
    bool v = false;
    if (parse_bool(value, &v)) {
      config.tail.reverse = v;
      result.applied = true;
    } else {
      reject(result.warning, rejected, key, value);
    }
    return result;
  }
  if (std::strcmp(key, "ds4.enabled") == 0) {
    // LED-L010 DualShock 4 provider enable (only consumed by the esp32_eq_ds4
    // build; the canonical env has no Bluetooth code and ignores the key).
    bool v = false;
    if (parse_bool(value, &v)) {
      config.ds4_enabled = v;
      result.applied = true;
    } else {
      reject(result.warning, rejected, key, value);
    }
    return result;
  }

  reject(result.warning, unknown, key, value);
  return result;
}

}  // namespace ledwall
