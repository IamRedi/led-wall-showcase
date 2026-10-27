#include <cstring>

#include "core/config.h"
#include "test_framework.h"

// Frozen spec §18: config loads with safe fallbacks; unknown keys are ignored
// with a warning; invalid values fall back without mutation.

void test_config_defaults() {
  const ledwall::AppConfig c;
  CHECK(c.orientation.flip_odd_rows == true);   // heritage EQ_FLIP_ODD_ROWS 1
  CHECK(c.orientation.flip_y == false);         // heritage EQ_FLIP_Y 0
  CHECK_EQ_INT(c.brightness_default_level, 40); // heritage EQ_BRIGHTNESS (spec §2)
  CHECK_EQ_INT(c.brightness_ceiling, 40);       // ADR-011 conservative default
  CHECK_EQ_INT(static_cast<int>(c.boot_base_mode), static_cast<int>(ledwall::ModeKind::Idle));
}

void test_config_valid_entries() {
  ledwall::AppConfig c;
  ledwall::ConfigEntryResult r = ledwall::apply_config_entry(c, "orientation.flip_odd_rows", "0");
  CHECK(r.applied);
  CHECK(!c.orientation.flip_odd_rows);
  CHECK(r.warning[0] == '\0');

  r = ledwall::apply_config_entry(c, "orientation.flip_y", "1");
  CHECK(r.applied);
  CHECK(c.orientation.flip_y);

  r = ledwall::apply_config_entry(c, "brightness.ceiling", "80");
  CHECK(r.applied);
  CHECK_EQ_INT(c.brightness_ceiling, 80);

  r = ledwall::apply_config_entry(c, "brightness.default_level", "0");
  CHECK(r.applied);
  CHECK_EQ_INT(c.brightness_default_level, 0);

  r = ledwall::apply_config_entry(c, "boot.base_mode", "expression");
  CHECK(r.applied);
  CHECK_EQ_INT(static_cast<int>(c.boot_base_mode), static_cast<int>(ledwall::ModeKind::Expression));
}

void test_config_unknown_key_warning() {
  ledwall::AppConfig c;  // defaults
  // "wifi.ssid" became a known key in LED-L003; use a never-defined key.
  const ledwall::ConfigEntryResult r = ledwall::apply_config_entry(c, "widget.foo", "home");
  CHECK(!r.applied);       // ignored, not stored
  CHECK(r.warning[0] != '\0');
  CHECK(std::strstr(r.warning, "unknown") != nullptr);
  CHECK_EQ_INT(c.brightness_ceiling, 40);  // nothing mutated
}

void test_config_invalid_values_fallback() {
  ledwall::AppConfig c;

  auto entry_rejected = [&](const char* key, const char* value) {
    const ledwall::ConfigEntryResult r = ledwall::apply_config_entry(c, key, value);
    ++g_checks;
    if (r.applied || r.warning[0] == '\0') {
      ++g_failures;
      std::printf("FAIL %s:%d: %s=%s should be rejected with warning\n", __FILE__, __LINE__, key,
                  value);
    }
  };

  entry_rejected("brightness.ceiling", "abc");
  entry_rejected("brightness.ceiling", "101");
  entry_rejected("brightness.ceiling", "-5");
  entry_rejected("brightness.default_level", "250");
  entry_rejected("orientation.flip_odd_rows", "2");
  entry_rejected("orientation.flip_y", "yes");
  entry_rejected("boot.base_mode", "bogus");
  entry_rejected("boot.base_mode", "");

  // Safe fallback: nothing changed from defaults.
  CHECK_EQ_INT(c.brightness_ceiling, 40);
  CHECK_EQ_INT(c.brightness_default_level, 40);
  CHECK(c.orientation.flip_odd_rows == true);
  CHECK(c.orientation.flip_y == false);
  CHECK_EQ_INT(static_cast<int>(c.boot_base_mode), static_cast<int>(ledwall::ModeKind::Idle));
}

void test_config_wifi_and_api_keys() {
  ledwall::AppConfig c;  // defaults: no wifi, ports 80/81
  CHECK_EQ_INT(c.http_port, 80);
  CHECK_EQ_INT(c.ws_port, 81);
  CHECK_EQ_INT(c.wifi_ssid[0], 0);
  CHECK_EQ_INT(c.wifi_password[0], 0);

  auto r = ledwall::apply_config_entry(c, "wifi.ssid", "wall-net");
  CHECK(r.applied);
  CHECK(std::strcmp(c.wifi_ssid, "wall-net") == 0);

  r = ledwall::apply_config_entry(c, "wifi.password", "hunter2!");
  CHECK(r.applied);
  CHECK(std::strcmp(c.wifi_password, "hunter2!") == 0);

  r = ledwall::apply_config_entry(c, "api.http_port", "8080");
  CHECK(r.applied);
  CHECK_EQ_INT(c.http_port, 8080);

  r = ledwall::apply_config_entry(c, "api.ws_port", "8181");
  CHECK(r.applied);
  CHECK_EQ_INT(c.ws_port, 8181);

  // Invalid values fall back without mutation (§18).
  r = ledwall::apply_config_entry(c, "api.http_port", "0");
  CHECK(!r.applied);
  CHECK_EQ_INT(c.http_port, 8080);
  r = ledwall::apply_config_entry(c, "api.ws_port", "65536");
  CHECK(!r.applied);
  CHECK_EQ_INT(c.ws_port, 8181);
  r = ledwall::apply_config_entry(c, "api.ws_port", "abc");
  CHECK(!r.applied);

  // Oversized credentials rejected without truncation.
  char long_ssid[64];
  for (size_t i = 0; i < sizeof(long_ssid) - 1; ++i) long_ssid[i] = 'a';
  long_ssid[sizeof(long_ssid) - 1] = '\0';
  r = ledwall::apply_config_entry(c, "wifi.ssid", long_ssid);  // 63 chars > 32
  CHECK(!r.applied);
  CHECK(std::strcmp(c.wifi_ssid, "wall-net") == 0);
  r = ledwall::apply_config_entry(c, "wifi.password", long_ssid);  // 63 chars < 64 → valid
  CHECK(r.applied);
  CHECK(std::strlen(c.wifi_password) == 63);

  // Empty SSID clears the stored value (expressible "no wifi" state).
  r = ledwall::apply_config_entry(c, "wifi.ssid", "");
  CHECK(r.applied);
  CHECK_EQ_INT(c.wifi_ssid[0], 0);
}
