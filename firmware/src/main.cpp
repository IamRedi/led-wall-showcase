#include <Arduino.h>

#include "core/runtime.h"

namespace {
ledwall::Runtime g_runtime;
}

// Heritage serial diagnostics remain the bring-up channel (frozen spec §9:
// serial is diagnostic only, never the product contract).
void setup() {
  Serial.begin(115200);
  g_runtime.begin(millis());
}

void loop() {
  g_runtime.tick(millis());
}
