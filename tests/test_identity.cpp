#include "api/identity.h"
#include "test_framework.h"

// §10 identity/capability handshake: exact frozen field values and the
// conditional-capability rule (spectrum only if hardware confirmed).

void test_identity_payload_fields() {
  const ledwall::Identity id = ledwall::build_identity("led-wall-4d2f1a", "0.1.0-alpha.2", false);
  CHECK_STR_EQ(id.device_id, "led-wall-4d2f1a");
  CHECK_STR_EQ(id.device_type, "visual_display_v1");
  CHECK_STR_EQ(id.role, "display");
  CHECK_EQ_INT(id.protocol_version, 1);
  CHECK_STR_EQ(id.fw_version, "0.1.0-alpha.2");
  CHECK_EQ_INT(id.matrix_width, 30);
  CHECK_EQ_INT(id.matrix_height, 10);
}

void test_identity_core_capabilities_always_advertised() {
  const ledwall::Identity id = ledwall::build_identity("led-wall-x", "0.2.0-alpha.1", false);
  // §10: the §5 core set is always advertised, first and unchanged.
  CHECK_EQ_INT(id.capability_count,
               ledwall::kCoreCapabilityCount + ledwall::kExtendedCapabilityCount);
  const char* frozen[8] = {"display.text.show",     "display.text.clear",
                           "display.scene.set",     "display.animation.set",
                           "display.expression.set", "display.clear",
                           "display.brightness.set", "display.state.read"};
  for (uint8_t i = 0; i < 8; ++i) {
    CHECK(std::strcmp(id.capabilities[i], frozen[i]) == 0);
  }
  // LED-L009 implemented extensions + the LED-L010 zone capability
  // (capability discovery; appended in this order).
  const char* extended[9] = {"display.timer",  "display.games",  "display.menu",
                             "display.input",  "display.notify", "display.standby",
                             "display.visualizer", "text.styles", "display.zones"};
  for (uint8_t i = 0; i < 9; ++i) {
    CHECK(std::strcmp(id.capabilities[8 + i], extended[i]) == 0);
  }
}

void test_identity_conditional_spectrum_gate() {
  // §5/§14: spectrum advertised ONLY when mic hardware is confirmed.
  const ledwall::Identity without = ledwall::build_identity("led-wall-x", "v", false);
  CHECK_EQ_INT(without.capability_count,
               ledwall::kCoreCapabilityCount + ledwall::kExtendedCapabilityCount);
  const ledwall::Identity with = ledwall::build_identity("led-wall-x", "v", true);
  CHECK_EQ_INT(with.capability_count,
               ledwall::kCoreCapabilityCount + ledwall::kExtendedCapabilityCount + 2);
  const uint8_t base = ledwall::kCoreCapabilityCount + ledwall::kExtendedCapabilityCount;
  CHECK(std::strcmp(with.capabilities[base], "display.spectrum.start") == 0);
  CHECK(std::strcmp(with.capabilities[base + 1], "display.spectrum.stop") == 0);
}
