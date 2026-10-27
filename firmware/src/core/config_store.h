#pragma once

#include <cstdint>

#include "core/config.h"
#include "core/warnings.h"

namespace ledwall {

// B-L010-R1 (repair cycle 1): host-testable seam over the device key-value
// store. The device reader (Arduino NVS/Preferences, config_store.cpp)
// implements this; host tests implement fakes. This is the same seam pattern
// that keeps the dispatcher host-testable — the review found the LED-L010
// keys were accepted by the applier but never fetched because the load path
// was device-only; the pure path below is now exercised by tests.
class ConfigStoreReader {
 public:
  virtual ~ConfigStoreReader() = default;
  virtual bool has_key(const char* key) const = 0;
  // Reads the stored string value. Returns false when the value is missing or
  // exceeds cap-1 (a truncated read is treated as no value; the applier then
  // keeps the default / warns through the normal path).
  virtual bool get_string(const char* key, char* out, uint32_t cap) const = 0;
};

// The canonical NVS storage-key list (dotted config vocabulary, frozen §18).
// The load path fetches EXACTLY these keys in this order; the completeness of
// this list against CONTRACT.md §18 is regression-pinned by the host battery
// (the omission that caused B-L010-R1 is exactly a key missing here).
extern const char* const kConfigStorageKeys[];
extern const uint8_t kConfigStorageKeyCount;

// Pure config load path (host-testable): starts from spec defaults and, for
// every documented storage key present in `reader`, applies the stored value
// through apply_config_entry. Missing keys keep the defaults silently;
// invalid values are ignored with a warning that surfaces in warnings[]
// (frozen spec §18). Stored secrets (wifi.password) are loadable but never
// reported through state, identity, or events.
bool load_config_entries(AppConfig* out, Warnings* warnings, const ConfigStoreReader* reader);

// Device-side entry point (Arduino NVS/Preferences-backed reader; device
// glue). Saving config changes arrives with the first config-mutating API
// command (future milestone); provisioning today is the documented local NVS
// image path (tools/nvs_provision.py).
void load_config(AppConfig* out, Warnings* warnings);

}  // namespace ledwall
