#pragma once

#include <cstdint>

#include "render/display_topology.h"

namespace ledwall {
namespace effects {

// LED-L010 curated 1D effect library (ADR-LED-L010 D3; expansion brief §16).
// Every renderer is a pure function of (tail, t_ms): it clears the tail,
// draws the complete visual for that instant, writes only through the
// bounds-checked TailFramebuffer, and is deterministic for the same time
// input (t_ms = program-relative render time, wrap-safe unsigned difference).
// Colors route through the deterministic color.h HSV conversion exactly like
// the matrix renderers (host goldens == device output).

// Tail-native set (registry target: tail):
void tail_render_comet(TailFramebuffer& tail, uint32_t t_ms);
void tail_render_scanner(TailFramebuffer& tail, uint32_t t_ms);
void tail_render_meteor(TailFramebuffer& tail, uint32_t t_ms);
void tail_render_spark_trail(TailFramebuffer& tail, uint32_t t_ms);
void tail_render_fire_flow(TailFramebuffer& tail, uint32_t t_ms);

// Combined-capable set (registry targets: tail and both — the matrix
// counterparts live in animation_renderers.cpp and are orchestrated by the
// renderer for target "both"):
void tail_render_pulse(TailFramebuffer& tail, uint32_t t_ms);
void tail_render_breathing(TailFramebuffer& tail, uint32_t t_ms);
void tail_render_lightning(TailFramebuffer& tail, uint32_t t_ms);
void tail_render_progress(TailFramebuffer& tail, uint32_t t_ms);
void tail_render_countdown_progress(TailFramebuffer& tail, uint32_t t_ms);
void tail_render_chase(TailFramebuffer& tail, uint32_t t_ms);
void tail_render_color_wave(TailFramebuffer& tail, uint32_t t_ms);
void tail_render_heartbeat(TailFramebuffer& tail, uint32_t t_ms);

// Tail version of the existing ambient rainbow_flow (registry: matrix default,
// tail/both available).
void tail_render_rainbow_flow(TailFramebuffer& tail, uint32_t t_ms);

using TailRenderer = void (*)(TailFramebuffer&, uint32_t);

// Zero-based logical cell used by the one-hot scanner at a program-relative time.
uint16_t tail_scanner_position(uint16_t length, uint32_t t_ms);

// Registry-name → tail renderer binding. Returns nullptr for names with no
// tail form (fail closed → blank tail upstream).
TailRenderer tail_animation_by_name(const char* name);

// ---- Parameterized cores shared with the combined semantics (§16/§20) ----

// Whole-tail brightness pulse in one hue (expressions wash, notification
// accent, spectrum bass). `val_min`..255 brightness band around sin_u8.
void tail_pulse_core(TailFramebuffer& tail, uint32_t t_ms, uint8_t hue, uint8_t val_min,
                     uint32_t period_ms_divisor);

// Fill 0..`frac`(0..255) of the tail from cell 0 in `fg` over a dim `bg`.
void tail_progress_core(TailFramebuffer& tail, uint8_t frac, Rgb fg, Rgb bg);

// White-blue strobe window (lightning combined use): renders `strokes` quick
// flashes inside a `window_ms` period; dark outside the window.
void tail_lightning_core(TailFramebuffer& tail, uint32_t t_ms, uint32_t window_ms);

// Directional chase dots with a hue offset (alert red chase: hue 0).
void tail_chase_core(TailFramebuffer& tail, uint32_t t_ms, uint8_t hue_base);

}  // namespace effects
}  // namespace ledwall
