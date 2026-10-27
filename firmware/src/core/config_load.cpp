#include "core/config_store.h"

#include <cstring>

namespace ledwall {

// The canonical NVS storage-key list (dotted config vocabulary, frozen §18,
// CONTRACT.md §18). B-L010-R1 (repair cycle 1): the four LED-L010 keys join
// the load path — they were accepted by apply_config_entry since LED-L010 but
// never fetched, so the tail could never be enabled on hardware and the
// wiring handoff was inoperative. Order is irrelevant to semantics but kept
// stable for reviewability; the host battery pins this list against the
// contract so the two can never silently diverge again.
const char* const kConfigStorageKeys[] = {
    "orientation.flip_odd_rows",
    "orientation.flip_y",
    "brightness.default_level",
    "brightness.ceiling",
    "boot.base_mode",
    "wifi.ssid",
    "wifi.password",  // stored secret, never reported
    "api.http_port",
    "api.ws_port",
    "input.enabled",  // LED-L009 (default: off)
    "tail.enabled",   // LED-L010 (default: off until the Owner wires the rope)
    "tail.length",    // LED-L010 (1..300; default 60 = documented PLACEHOLDER)
    "tail.reverse",   // LED-L010 (default: off)
    "ds4.enabled",    // LED-L010 (consumed only by the esp32_eq_ds4 build)
};
const uint8_t kConfigStorageKeyCount = static_cast<uint8_t>(
    sizeof(kConfigStorageKeys) / sizeof(kConfigStorageKeys[0]));

bool load_config_entries(AppConfig* out, Warnings* warnings, const ConfigStoreReader* reader) {
  if (out == nullptr || reader == nullptr) {
    return false;
  }
  *out = AppConfig{};  // spec defaults; storage overrides below
  char value[128];
  for (uint8_t i = 0; i < kConfigStorageKeyCount; ++i) {
    const char* key = kConfigStorageKeys[i];
    if (!reader->has_key(key) || !reader->get_string(key, value, sizeof(value))) {
      continue;  // missing key → spec default stands (no warning, frozen §18)
    }
    const ConfigEntryResult result = apply_config_entry(*out, key, value);
    if (!result.applied && result.warning[0] != '\0') {
      add_warning(warnings, result.warning);
    }
  }
  return true;
}

}  // namespace ledwall
