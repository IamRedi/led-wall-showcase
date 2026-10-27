#include "render/led_driver.h"

#include <FastLED.h>

#include "device_constants.h"

namespace ledwall {

namespace {
CRGB g_leds[LED_WALL_LED_COUNT];
// LED-L010 tail controller buffer. Registered with FastLED only when the tail
// is enabled (config default: disabled — the accepted baseline image behavior).
CRGB g_tail[TailFramebuffer::kMaxLength];
bool g_tail_enabled = false;
uint16_t g_tail_length = 0;
bool g_tail_reverse = false;
}  // namespace

// Orientation flips live in matrix_mapping (pixel-address time); the driver
// only owns strip wiring. L002 renders the safe blank program, so no
// pixel-write call site exists yet.
void led_driver_begin(const OrientationFlags& orientation) {
  (void)orientation;
  FastLED.addLeds<WS2812, LED_WALL_DATA_PIN, GRB>(g_leds, LED_WALL_LED_COUNT);
}

void led_driver_begin_tail(const TailZoneConfig& tail) {
  g_tail_enabled = false;
  g_tail_length = 0;
  g_tail_reverse = tail.reverse;
  if (!tail.enabled || tail.length < 1 || tail.length > TailFramebuffer::kMaxLength) {
    return;  // disabled or invalid: no controller, no pins touched beyond begin
  }
  FastLED.addLeds<WS2812, LED_WALL_TAIL_DATA_PIN, GRB>(g_tail, tail.length);
  g_tail_enabled = true;
  g_tail_length = tail.length;
}

void led_driver_set_brightness(uint8_t scale) { FastLED.setBrightness(scale); }

void led_driver_clear() { FastLED.clear(false); }

void led_driver_clear_tail() {
  if (!g_tail_enabled) {
    return;
  }
  for (uint16_t i = 0; i < g_tail_length; ++i) {
    g_tail[i] = CRGB(0, 0, 0);
  }
}

void led_driver_show() { FastLED.show(); }

// LED-L007: the render layer's framebuffer crosses into FastLED's CRGB
// framebuffer here — the only device-specific pixel conversion.
void led_driver_blit(const Rgb* pixels, uint8_t output_scale) {
  for (uint16_t i = 0; i < LED_WALL_LED_COUNT; ++i) {
    g_leds[i] = CRGB(static_cast<uint8_t>(pixels[i].r * output_scale / 255u),
                     static_cast<uint8_t>(pixels[i].g * output_scale / 255u),
                     static_cast<uint8_t>(pixels[i].b * output_scale / 255u));
  }
}

// LED-L010: tail blit with the physical reverse mapping applied at the driver
// boundary (logical cell 0 = the configured start of the rope).
void led_driver_blit_tail(const Rgb* cells, uint16_t count, uint8_t output_scale) {
  if (!g_tail_enabled || cells == nullptr) {
    return;
  }
  const uint16_t n = (count < g_tail_length) ? count : g_tail_length;
  for (uint16_t i = 0; i < n; ++i) {
    const uint16_t physical = g_tail_reverse ? static_cast<uint16_t>(g_tail_length - 1 - i) : i;
    g_tail[physical] = CRGB(static_cast<uint8_t>(cells[i].r * output_scale / 255u),
                            static_cast<uint8_t>(cells[i].g * output_scale / 255u),
                            static_cast<uint8_t>(cells[i].b * output_scale / 255u));
  }
}

}  // namespace ledwall
