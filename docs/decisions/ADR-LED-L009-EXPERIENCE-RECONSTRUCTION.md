# ADR-LED-L009 — Product Experience Reconstruction

- **Status:** Accepted architecture for the LED-L009 milestone (locally implemented + verified; independent review pending)
- **Date:** 2026-10-10
- **Owner:** Redion Cekrezi
- **Task:** LED-L009 (see `docs/work/TASKS.yaml`)

## Context

The LED Wall (LED-L001..L007 accepted, LED-L008 hardware campaign repairs carried in the
baseline) is a working 30×10 WS2812 display with a semantic HTTP/WS API, a functional but
engineering-oriented dashboard, host-side-only countdown driving, no physical controls, no
local menu, no games, and a text pipeline locked to uppercase heritage glyphs. The Owner
authorized ONE coherent reconstruction milestone: premium phone UI, Text Studio, a real
device-side countdown, unified physical input, a local pixel menu, five classic games,
curated visual expansion, a generic visualizer input (microphone explicitly deferred),
semantic notifications, and standby — with every accepted client still working.

## Decisions

### D1 — ESP32 owns device truth; the phone renders presentation

All new state (countdown, games, menu, standby, notifications, visualizer) lives on the
device as first-class runtime subsystems and runs with zero clients connected. The
dashboard is a pure same-origin semantic-API client (LED-L004 boundary preserved): it
performs preview, layout, animation of controls, and local interpolation only — never
authority. The device exposes exact milliseconds (`timer.remainingMs`); the UI advances
the countdown locally between fetches and re-syncs on every WS event.

### D2 — Semantic modes extend the §4 mode set additively

`ModeKind` gains `menu`, `timer`, `game`, `notification`, `standby` (append-only; the
seven §4 kinds and names unchanged). New modes reuse the accepted §11 semantics: a
notification is a normal transient (expiry restores the base; CLEAR-1 cancels), standby
is a persistent base mode, waking is simply replacing the base. The no-stack rule stands:
entering the menu replaces the base; exiting lands on idle. Games keep their paused state
and can be re-entered, but the machine does not resurrect display state.

### D3 — One unified input model

Normalized events (`up/down/left/right/confirm/back/start_pause/home`) terminate at a
single bounded queue (`InputEventQueue`, 16 slots, overflow counted). Providers: the
physical GPIO layer (disabled by default — the joystick shield pinout is NOT evidenced
in any repository source; candidate map documented in `input/input_pins.h`, enabled via
config `input.enabled`) and the API (`POST /display/input`, the phone-as-controller).
Menu, games, timer one-button semantics, and standby wake all consume the same events.
No game or menu code ever reads a GPIO.

### D4 — Countdown is an engine, the display is a mode

`CountdownTimer` (pure, wrap-safe, R-B01/R-B02 contracts) holds
IDLE/RUNNING/PAUSED/FINISHED with exact-millisecond pause freezes. `timer.start` shows
the countdown (base becomes `timer`); the engine keeps counting in any display mode;
`timer.reset` re-arms in place; `timer.stop` re-arms AND leaves the timer display — the
semantically distinct pair. Display formats: `MM:SS` ≥ 60 s, `SS` ≥ 10 s, `S.mmm` < 10 s
(all ≤ 5 glyphs, static-renderable; duration capped at 99:59 for exactly that reason).
`timer.done` is a new WS event.

### D5 — Text Studio semantics, not renderer internals

`text.show` gains optional `mode` (`fixed`/`marquee`/`words`), `direction` (`rtl`/`ltr`
— RTL is the product default reading direction; the heritage `scroll:true` flag maps to
an LTR marquee byte-exactly, preserving every LED-L007 golden), `loop`, `wordDwellMs`
(200..60000), `style` (`solid`/`color_bg`/`rainbow_text`/`rainbow_bg`) and `color`/
`bgColor` over a 13-name semantic palette (index 12 = the accepted heritage green).
`textTooWide` applies to Fixed only. The charset extends to full lowercase, Albanian
`Ë ë Ç ç` (decoded UTF-8 code points) and 17 punctuation marks; `skippedCharacters`
now counts unsupported code points (one space each) — resolving the byte-vs-codepoint
ambiguity recorded as O-L007-R5, documented in CONTRACT.md.

### D6 — Games: deterministic framework, curated five

`GameHost` drives one game at a time through Ready → Running ⇄ Paused → Over with
fixed-cadence stepping (100 ms logic, bounded catch-up), immediate event dispatch
(ms-precise for the reaction game), bounded static state (no heap), xorshift PRNG with
fixed seeds (deterministic, host-golden-testable), and FrameBuffer-only rendering.
Games: `pong`, `snake`, `breakout`, `reaction` (stop-at-zero), `dodger`. Back/Home exits
via the runtime; no game requires the phone.

### D7 — Visualizer input now, microphone later

`SpectrumInput` accepts exactly 30 normalized band values (`POST /display/visualizer/
data`), rate-limited to one feed per 25 ms, expiring after 1500 ms of silence into a
deterministic decay to blank. Source-agnostic by contract: phone, PC, HUGO, HA or a
future ESP32-S3 microphone bridge are indistinguishable. No microphone, no FFT, no ADC
code exists in this milestone (frozen spec §14/ADR-012 stay honored);
`display.visualizer` is advertised, `spectrum.available` stays false (that flag is
reserved for confirmed mic hardware).

### D8 — Capability discovery over adapter rewrites

The extended registries (animations 8→23, expressions 8→12, games 5, inputs 8, styles 4,
palette 13) are discoverable via `GET /registry` and the extended `capabilities[]`. The
§5 core capability list is byte-unchanged and first. HA/HUGO adapters keep working
unchanged (their embedded vocabularies remain a valid subset); adapter updates are
explicitly deferred work.

### D9 — Inactive sound seam

Semantic cues (`TIMER_DONE`, `GAME_START`, `GAME_OVER`, `NOTIFICATION`, `BUTTON`) flow
through a `SoundSink` interface whose only implementation is a no-op: no pin claims, no
hardware claims, capability not advertised. A future speaker milestone implements a real
sink behind the same seam.

### D10 — Protected repairs stay byte-similar

The five LED-L008 hardware fixes (WiFi power/sleep/reconnect pairing, dotted NVS keys,
content-change render requests, time-varying continuous re-render, corrected D/M/R
glyphs) are preserved verbatim in the reconstruction (runtime.cpp carries the same
bracketed comments) and are pinned by regression tests (corrected glyph goldens, live
reboot-recovery re-verification). The only additive change to the render gate is
`force_render_` — a one-shot render request for timer state flips that do not touch the
machine/brightness.

### D11 — Deferred with recorded limitation: DualShock 4

The board is a classic-ESP32 `esp32dev` with WiFi at 71.2 % flash and the workload
documented above. A BT-Classic HID host stack (Bluepad32-class) requires toolchain-level
dependency changes and a substantial RAM/flash budget with dedicated stability
validation — not safely supportable inside this milestone's budget. Per the brief's
explicit fallback, DS4 support is NOT implemented and NOT faked; the unified input
abstraction (D3) is the migration point for a future ESP32-S3 controller milestone.

## Consequences

- Flash 67.2 % → 71.2 % (+52.5 kB), RAM 15.0 % → 15.4 % (+1.4 kB) — safe margins,
  measured before/after.
- The dashboard grows to a 39 KB PROGMEM asset (was 26.8 KB) — same bounded serving
  class, ASCII-only, no external resources.
- `skippedCharacters` semantics change (bytes → code points) is a disclosed,
  contract-documented update of accepted behavior (O-L007-R5 resolution path).
- LED-L008 stays `incomplete` (never accepted); its campaign history is preserved and
  its repairs are protected in this baseline.
- Microphone, speaker, DS4 and ESP32-S3 migration remain future milestones behind
  documented seams (`SpectrumInput`, `SoundSink`, `InputEventQueue`, device constants).

## Verification anchors

Host battery 30810/0 C++ (+ ASAN/UBSAN clean), 31/0 node JS-core, HA 107/0 + HUGO
158/0 adapters unchanged; firmware `cb571c40…` (939472 B) with byte-identical clean
rebuild; bounded live verification on the physical device (see
`docs/work/evidence/LED-L009-IMPLEMENTATION.md`).
