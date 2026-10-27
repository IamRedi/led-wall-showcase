#include "api/identity.h"

#include <cstring>

namespace ledwall {

Identity build_identity(const char* device_id, const char* fw_version, bool spectrum_available,
                        bool ds4_compiled) {
  Identity id;
  if (device_id != nullptr) {
    std::strncpy(id.device_id, device_id, sizeof(id.device_id) - 1);
    id.device_id[sizeof(id.device_id) - 1] = '\0';
  }
  if (fw_version != nullptr) {
    std::strncpy(id.fw_version, fw_version, sizeof(id.fw_version) - 1);
    id.fw_version[sizeof(id.fw_version) - 1] = '\0';
  }
  for (uint8_t i = 0; i < kCoreCapabilityCount; ++i) {
    id.capabilities[id.capability_count++] = kCoreCapabilities[i];
  }
  // LED-L009 implemented extensions (always advertised — capability
  // discovery; the visualizer is an external-input contract, the microphone
  // itself remains unimplemented/deferred).
  for (uint8_t i = 0; i < kExtendedCapabilityCount; ++i) {
    id.capabilities[id.capability_count++] = kExtendedCapabilities[i];
  }
  if (spectrum_available) {
    // Conditional per §5/§14: advertised only when mic hardware is confirmed.
    id.capabilities[id.capability_count++] = "display.spectrum.start";
    id.capabilities[id.capability_count++] = "display.spectrum.stop";
  }
  if (ds4_compiled) {
    // LED-L010 (repair cycle 1, M-L010-4): the DS4 flag is the compile-time
    // truth of the esp32_eq_ds4 build (kDs4ProviderCompiled) — the capability
    // is advertised only where the provider is compiled, and the array is
    // sized for this worst case (see kConditionalCapabilityCount).
    id.capabilities[id.capability_count++] = "display.ds4";
  }
  return id;
}

}  // namespace ledwall
