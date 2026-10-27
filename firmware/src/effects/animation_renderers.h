#pragma once

#include <cstdint>

#include "render/framebuffer.h"

namespace ledwall {
namespace effects {

// The 8 registered animation renderers (frozen spec §7) — COPY/ADAPT of the
// heritage `PLUTO EQ ACCESSORY/src/main.cpp` demo functions. Each renderer is
// a pure function of (frame, t_ms): it clears the frame, draws the complete
// visual for that instant, writes only through FrameBuffer (bounds-checked),
// and is deterministic for the same time input. `t_ms` is the
// program-relative render time (wrap-safe unsigned difference).
//
// Heritage color calls (CHSV) are routed through the deterministic color.h
// equivalents; direct CRGB constants are copied byte-for-byte.

void render_corners(FrameBuffer& frame, uint32_t t_ms);
void render_checker(FrameBuffer& frame, uint32_t t_ms);
void render_border(FrameBuffer& frame, uint32_t t_ms);
void render_row_sweep(FrameBuffer& frame, uint32_t t_ms);
void render_bounce(FrameBuffer& frame, uint32_t t_ms);
void render_wave(FrameBuffer& frame, uint32_t t_ms);
void render_rain(FrameBuffer& frame, uint32_t t_ms);
void render_plasma(FrameBuffer& frame, uint32_t t_ms);

// LED-L009 curated additions (reconstruction brief §15). Icon/character set:
void render_smile(FrameBuffer& frame, uint32_t t_ms);
void render_wink(FrameBuffer& frame, uint32_t t_ms);
void render_heart(FrameBuffer& frame, uint32_t t_ms);
void render_heart_pulse(FrameBuffer& frame, uint32_t t_ms);
void render_eyes(FrameBuffer& frame, uint32_t t_ms);
void render_loading(FrameBuffer& frame, uint32_t t_ms);
void render_success(FrameBuffer& frame, uint32_t t_ms);
void render_warning(FrameBuffer& frame, uint32_t t_ms);
void render_arrows(FrameBuffer& frame, uint32_t t_ms);
// Ambient set:
void render_aurora(FrameBuffer& frame, uint32_t t_ms);
void render_stars(FrameBuffer& frame, uint32_t t_ms);
void render_rainbow_flow(FrameBuffer& frame, uint32_t t_ms);
void render_fire(FrameBuffer& frame, uint32_t t_ms);
void render_matrix(FrameBuffer& frame, uint32_t t_ms);
void render_calm(FrameBuffer& frame, uint32_t t_ms);

// LED-L010 both-surface counterparts (ADR-LED-L010 D3): the matrix visuals
// paired with the tail renderers in tail_renderers.h — rendered together when
// an animation is commanded target "both".
void render_pulse_matrix(FrameBuffer& frame, uint32_t t_ms);
void render_breathing_matrix(FrameBuffer& frame, uint32_t t_ms);
void render_lightning_matrix(FrameBuffer& frame, uint32_t t_ms);
void render_progress_matrix(FrameBuffer& frame, uint32_t t_ms);
void render_countdown_progress_matrix(FrameBuffer& frame, uint32_t t_ms);
void render_chase_matrix(FrameBuffer& frame, uint32_t t_ms);
void render_color_wave_matrix(FrameBuffer& frame, uint32_t t_ms);
void render_heartbeat_matrix(FrameBuffer& frame, uint32_t t_ms);

// Parameterized wave/plasma cores shared with the §8 expression mapping
// (heritage algorithms; `time_divisor` is the heritage phase divisor, the
// registered defaults are 80 and 25).
void render_wave_frame(FrameBuffer& frame, uint32_t t_ms, uint32_t time_divisor,
                       uint8_t hue_offset);
void render_plasma_frame(FrameBuffer& frame, uint32_t t_ms, uint32_t time_divisor);

using AnimationRenderer = void (*)(FrameBuffer&, uint32_t);

// Registry-name → renderer binding (frozen §7 names first, then the LED-L009
// additions in registry order; bindings are internal data). Returns nullptr
// for unknown names (fail closed → blank).
AnimationRenderer animation_by_name(const char* name);

// True iff the named animation's frame content changes over time. The
// heritage `corners` and the static icons (smile, heart, success, warning)
// are time-independent; everything else animates. Unknown names: false.
bool animation_is_time_varying(const char* name);

}  // namespace effects
}  // namespace ledwall
