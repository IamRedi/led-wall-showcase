#include "core/config_store.h"

#include <cstring>

#include <Preferences.h>

namespace ledwall {

namespace {

constexpr const char* kNamespace = "ledwall";

// Device-side ConfigStoreReader over the Arduino NVS (Preferences). Storage
// keys use the product's dotted config vocabulary (config.cpp
// apply_config_entry): D-L008-2 (LED-L008 campaign) — underscore spellings
// were rejected by the applier and could never apply; B-L010-R1 (repair
// cycle 1) — the LED-L010 keys were applier-valid but never FETCHED; both
// classes of gap are now impossible: the fetch list is the pure, host-pinned
// kConfigStorageKeys list in config_load.cpp.
class PreferencesReader final : public ConfigStoreReader {
 public:
  explicit PreferencesReader(Preferences* prefs) : prefs_(prefs) {}

  bool has_key(const char* key) const override { return prefs_->isKey(key); }

  bool get_string(const char* key, char* out, uint32_t cap) const override {
    if (cap == 0) {
      return false;
    }
    // Preferences getString reads into a String; copy bounded. A value longer
    // than the config vocabulary ever stores is treated as no value (the
    // caller keeps the default — same fail-safe as a missing key).
    const String value = prefs_->getString(key, "");
    if (value.length() >= cap) {
      return false;
    }
    std::memcpy(out, value.c_str(), value.length() + 1);
    return true;
  }

 private:
  Preferences* prefs_;
};

}  // namespace

void load_config(AppConfig* out, Warnings* warnings) {
  if (out == nullptr) {
    return;
  }
  Preferences prefs;
  prefs.begin(kNamespace, true);  // read-only
  PreferencesReader reader(&prefs);
  load_config_entries(out, warnings, &reader);
  prefs.end();
}

}  // namespace ledwall
