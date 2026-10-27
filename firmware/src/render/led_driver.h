#pragma once

#include <cstdint>

#include "render/display_topology.h"
#include "render/matrix_mapping.h"
#include "render/pixel.h"

namespace ledwall {

// Device-side LED strip driver (Arduino + FastLED). COPY/ADAPT of the heritage
// wiring in `PLUTO EQ ACCESSORY/src/main.cpp` setup(): WS2812 on the heritage
// data pin, GRB color order, 300 LEDs (frozen spec §2 VERIFIED facts).
//
// LED-L010 (ADR-LED-L010 D1/D2): the driver owns BOTH physical controller
// instances — the matrix on the heritage data pin and the tail on the
// audit-locked tail pin — plus the tail 1D mapping (including the configured
// physical reverse) and frame submission for both. Product logic never sees
// FastLED; it renders into FrameBuffer / TailFramebuffer and calls blit.
void led_driver_begin(const OrientationFlags& orientation);
// LED-L010: attach the tail controller. With `enabled=false` (the shipping
// default) no second controller is registered and every tail call below is a
// safe no-op — the matrix behaves exactly as the accepted baseline.
void led_driver_begin_tail(const TailZoneConfig& tail);
void led_driver_set_brightness(uint8_t scale);  // 0..255 (FastLED scale)
void led_driver_clear();                        // blank the framebuffer (no show)
void led_driver_clear_tail();                   // blank the tail buffer (no show)
void led_driver_show();                         // submit matrix (+tail) frame
// LED-L007: copy a rendered render-layer frame into the output framebuffer
// (RGB → CRGB); call before show().
void led_driver_blit(const Rgb* pixels, uint8_t output_scale = 255);
// LED-L010: copy the tail surface into the tail controller buffer, applying
// the configured physical reverse at the driver boundary. No-op when the tail
// is disabled.
void led_driver_blit_tail(const Rgb* cells, uint16_t count, uint8_t output_scale = 255);

}  // namespace ledwall
