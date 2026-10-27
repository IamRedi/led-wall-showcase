#pragma once

#include <cstdint>

#include "api/command.h"  // TextProgram (§4 text program model)
#include "core/mode_machine.h"
#include "games/game.h"
#include "menu/local_menu.h"
#include "pixel/pixel_canvas.h"
#include "render/display_topology.h"
#include "render/framebuffer.h"
#include "timer/countdown.h"
#include "visualizer/spectrum_input.h"

namespace ledwall {
namespace renderer {

// Renderer selection (frozen spec §4→§7/§8; LED-L009 extension): deterministically
// render the currently active semantic mode into the framebuffer. The input is the
// machine snapshot's CURRENT program (transient overrides base, §11) — the
// renderer owns no program state of its own and never bypasses the mode
// machine. Layering: no command parsing, no HTTP/WS/HA/HUGO knowledge, no
// brightness interaction (the pipeline applies brightness below the
// framebuffer), writes only through FrameBuffer (bounds-checked). The
// subsystem views are read-only for rendering; the runtime owns their state.
// LED-L010: the same program model renders BOTH zones — the matrix
// FrameBuffer and, when the tail exists, the 1D TailFramebuffer (per the
// commanded ZoneTarget and the combined semantics of ADR-LED-L010 D3).
struct RenderInput {
  ModeKind kind{ModeKind::Idle};
  const char* name{""};              // current program name ("" for idle/text)
  TextProgram* text{nullptr};  // set iff kind == Text (done-flag bookkeeping is runtime-owned)
  uint32_t started_ms{0};            // program-relative time origin (machine snapshot)
  uint32_t now_ms{0};                // current monotonic device time
  ZoneTarget target{ZoneTarget::Matrix};  // LED-L010 commanded zone (scene/animation)
  // LED-L009 subsystem views (set by the runtime for the matching kind):
  const CountdownTimer* timer{nullptr};   // kind == Timer
  const SpectrumInput* spectrum{nullptr};  // kind == Spectrum
  const MenuState* menu{nullptr};          // kind == Menu
  games::Game* game{nullptr};              // kind == Game (rendered read-only)
  const NotifyState* notify{nullptr};      // kind == Notification
  const PixelCanvas* pixel{nullptr};        // kind == Paint (V4.1 product canvas)
  uint32_t frame_id{0};  // one logical composition identity for both surfaces
};

// Renders the complete frame for `input`. Deterministic: the same input
// always produces the same frame. Unknown/unrenderable state (unknown
// registry name, paint without a renderer, missing subsystem view, out-of-range
// kind) renders the safe blank frame — fail closed, never garbage (frozen §12).
// LED-L010 two-surface form: `tail` may be nullptr (tail disabled — every tail
// write is skipped, matrix behavior identical to the accepted baseline).
void render(FrameBuffer& frame, TailFramebuffer* tail, const RenderInput& input);
// Matrix-only compatibility form (LED-L009 accepted signature).
void render(FrameBuffer& frame, const RenderInput& input);

// True iff the current program's frame content changes over time and must
// re-render through the accepted throttle; false for static programs (idle,
// static text, corners, static icons, paused timer, standby, ...) so the
// framebuffer never spams show() (frozen §12 bounded rendering).
bool program_is_time_varying(const RenderInput& input);

// §9 text.done: has the current text program completed one full display?
// Fixed text: once a frame of it has actually been shown. Marquee: after one
// full wrap traversal. Word-by-word: when a one-shot program has shown its
// last word (looping programs never complete). `frame_shown` reports whether
// a frame of the current program was rendered in this tick.
bool text_program_complete(const TextProgram& text, uint32_t now_ms, bool frame_shown);

// LED-L009 repair cycle 2 (V-L009-1): true iff the text program's frame
// content DIFFERS from the frame that was shown at `last_shown_ms`. At the
// 20 fps throttle floor a marquee whose speed exceeds 50 ms/px advances less
// than one pixel per frame, so blanket re-rendering re-emits byte-identical
// full-strip frames several times per pixel step — wasted work at best and a
// needless stress on the panel's recorded latch marginality (O-L008-2) at
// worst. The gate uses this to re-show a marquee only when its pixel offset
// (or a time-drifting rainbow hue, or the active word) actually changed.
// Motion smoothness is unchanged: sub-pixel redraws are visually identical.
bool text_frame_content_changed(const TextProgram& text, uint32_t now_ms,
                                uint32_t last_shown_ms);

}  // namespace renderer
}  // namespace ledwall
