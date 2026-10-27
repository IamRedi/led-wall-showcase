# ADR-LED-L010 — Matrix + Tail + DualShock 4 + 1P/2P Input + Premium Mobile UI V3

- **Status:** Authorized architecture for the LED-L010 milestone (in progress; locally
  implemented + verified state is the executor's endpoint — review/acceptance pending)
- **Date:** 2026-10-11
- **Owner:** Redion Cekrezi
- **Task:** LED-L010 (see `docs/work/TASKS.yaml`)
- **Basis:** the current KNOWN-WORKING device snapshot (LED-L009 repair-cycle-3 state,
  HEAD `cfb8c58…`, on-chip + locally rebuilt artifact `0f7a38de…` byte-identical,
  verified live 2026-10-11) and the Owner's expansion brief of the same date.

## Context

The LED Wall is a working integrated appliance after LED-L009: 30×10 WS2812 matrix on
GPIO13, semantic HTTP/WS API, premium six-section dashboard, Text Studio, device-side
countdown, unified input, local menu, five deterministic games, 23 animations,
12 expressions, visualizer input, notifications, standby. LED-L008 is truthfully
`incomplete` (never accepted; five hardware repairs protected in the baseline); LED-L009
is `verified` awaiting independent review — NOT accepted, and not recorded as accepted
here. The Owner authorized ONE expansion milestone: a second 1D display surface (~2 m
diffused tail), a real DualShock 4 controller provider, explicit P1/P2 player slots with
a reserved future wired-controller path, a curated tail/combined animation set, tail
games, and the Owner-approved Premium Mobile UI V3 — all additive, with the working
system preserved and recoverable.

## Decisions

### D1 — Two display zones under a topology boundary; product logic never touches FastLED

`render/display_topology.h` introduces the canonical zone model:

- `ZoneTarget { Matrix, Tail, Both }` — every animation/game render path declares or is
  commanded to a target.
- `TailFramebuffer` — bounded 1D logical surface (1..`kMaxTailLength`=300 cells,
  bounds-checked writes like FrameBuffer), the ONLY tail pixel surface product code
  sees.
- Tail config: `tail.enabled` (default **0** — ships disabled until the Owner wires and
  counts the physical rope), `tail.length` (1..300, default 60 — **placeholder, clearly
  recorded as UNVERIFIED until the Owner counts the actual rope**), `tail.reverse`
  (default 0).

The driver layer (`led_driver`) owns both physical controller instances (matrix CRGB
array on GPIO13 + tail CRGB array on the tail pin), the serpentine matrix mapping, the
tail 1D mapping incl. the reverse flag, the global brightness scale, and frame
submission. Matrix behavior is unchanged when the tail is disabled or absent; a tail
render into a disabled tail is a safe no-op. Portability seams for a future ESP32-S3 /
split-controller design are preserved (the product layer never includes FastLED; only
`led_driver.cpp` does — boundary unchanged from LED-L002).

### D2 — Tail data output pin: GPIO27 (audit-locked)

Complete GPIO audit of the firmware (the only pin-claiming sources are
`device_constants.h`, `input/input_pins.h`, `platformio.ini`):

| GPIO | Status |
|---|---|
| 0, 2, 5, 12, 15 | strapping — avoided (candidate input map already avoids) |
| 1, 3 | UART0 USB-serial console |
| 4 | **free** — receives the reassigned candidate `left` input (see below) |
| 6–11 | integrated flash — unusable |
| 13 | **MATRIX data (live, canonical)** |
| 14, 18, 19, 22, 23 | free (VSPI unused — FastLED WS2812 drives via RMT) |
| 16, 17 | **RESERVED: UART2 = future wired P1 RP2040 controller node** (freed from the candidate input map) |
| 25, 26, 32, 33, 21 | candidate input buttons (provider disabled, unverified wiring) |
| 27 | candidate `left` input (provider disabled, unverified wiring) → **reassigned: TAIL data** |
| 34, 35 | input-only ADC1: candidate joystick axes; 34 is the legacy microphone pin (no mic hardware) |
| 36, 39 | input-only, free |

**Decision: GPIO27 = TAIL data output.** The brief floated GPIO27 as a candidate; the
audit shows its only claim is the DISABLED, wiring-unverified candidate `left` button.
That candidate moves to GPIO4 (claim-free, input-capable with pull-up) in the same
change, and the candidate map stays disabled by default (`input.enabled` default 0).
GPIO16/17 are freed and RESERVED for the future wired P1 node; the documented P2 path is
UART1 remapped onto two of the free pins (e.g. 22/23) — chosen at that future milestone,
collision-free with everything above. The pin lives in ONE canonical boundary
(`device_constants.h` `LED_WALL_TAIL_DATA_PIN`); no render/game code contains a GPIO
literal. Final confirmation for permanent Owner wiring is reported in the implementation
record (§28 handoff).

### D3 — Semantic animation targets, not a parallel effect library

`animation.set` / `scene.set` gain an optional `"target": "matrix"|"tail"|"both"`
(additive; absent = the animation's default). Registry metadata declares each
animation's supported targets and default. The registry grows 23 → 36 additively
(frozen first-8 order untouched):

- `comet, scanner, meteor, spark_trail, fire_flow` — tail-only (1D-native).
- `pulse, breathing, lightning, progress, countdown_progress, chase, color_wave,
  heartbeat` — tail-capable AND both-capable (combined renders the effect on both
  surfaces).
- `rainbow_flow` (existing name) — gains tail+both capability (default matrix,
  byte-compatible with every existing client).
- All other existing names — matrix-only; requesting tail on them ⇒ `invalidParameter`.
- Requesting a tail-target while the tail is disabled ⇒ `ledUnavailable` (the reserved
  §9 code, activated for exactly this semantic, CONTRACT-documented).

Combined semantics (brief §16/§20) are integrated, not bolted on: timer mode renders a
tail progress bar (draining green → red pulsing when finished); high-priority
notifications drive a red tail chase (normal priority: one accent pulse); expressions
wash the tail in their semantic color (alert = red chase, error = solid red, sleeping =
dim rest, …) data-driven like the matrix mapping; the visualizer adds a bass-band pulse
on the tail; standby blanks the tail with the matrix.

### D4 — Explicit P1/P2 player slots on the unified input model

The normalized event model gains player attribution: queue entries become
`{player 1|2, event}` (the wire field on `POST /display/input` is optional, default 1 —
existing clients byte-compatible). Consumers that are inherently single-user (menu,
timer one-button, standby wake, notification dismissal) ignore the player. Games receive
`on_input(player, event)`; per-game metadata declares supported player counts
(1, or {1,2}). Pong gains a genuine 2P mode (P1 left paddle, P2 right paddle; 1P keeps
the AI right side — existing behavior unchanged at players=1). Difficulty semantics are
untouched. Player mode is commanded at `game.set` (optional `"players"` field, validated
per game) and reported in `/state.game` + `/registry` per-game metadata; it applies at
the next start/restart exactly like difficulty.

### D5 — DualShock 4: real provider, isolated build env, honest coexistence evidence

The board is a classic-ESP32 `esp32dev`. A BT-Classic HID host (Bluepad32-class)
requires a toolchain-level core swap, which must NOT destabilize the proven WiFi
baseline image. Decision (as-built 2026-10-11):

- `[env:esp32_eq_ds4]` — a SECOND PlatformIO environment inheriting the canonical env,
  swapping `platform_packages` to the **Bluepad32 prebuilt Arduino core 4.1.0**
  (`esp32-arduino-lib-builder` release zip; the upstream ESP-IDF template route and the
  lib-builder git URL were evaluated and rejected with recorded reasons) and compiling
  with `LED_WALL_ENABLE_DS4=1`. The BT-Classic stack **costs ~630 KB flash (measured)**,
  so the env uses the `huge_app` partition layout (3 MB app; NVS stays at 0x9000 so
  stored credentials/config survive) — the default 1.25 MB app partition overflows at
  120.9 %. The canonical `esp32_eq` env (no BT code, no core swap, default partitions)
  remains the shipping image.
- Pure, host-tested core (`input/ds4_core.{h,cpp}`): DS4 state → normalized
  `PlayerEvent` stream (d-pad with auto-repeat 400 ms/150 ms; Cross→confirm,
  Circle→back, Square→left, Triangle→right, OPTIONS→start_pause, PS→home), and the
  controller lifecycle state machine (unpaired → pairing → connected{player slot} →
  disconnected → reconnecting) with first-connect = P1, second = P2.
- Device-only glue (`input/ds4_provider_bluepad32.cpp`, compiled only under
  `LED_WALL_ENABLE_DS4`): Bluepad32 v4 callback API (BP32.setup/update,
  ControllerPtr handles) → pure core → shared input queue. No game/menu code ever
  sees Sony button names (ADR-LED-L009 D3 boundary preserved).
- Identity advertises `display.ds4` ONLY in the DS4 build (capability truth: the
  canonical image does not claim controller support).
- **Coexistence outcome (honest):** the DS4 env builds and flashes cleanly, but on
  this exact board the BT-enabled image does NOT associate WiFi (blocker **DS4-1**,
  evidence in `docs/work/evidence/LED-L010-EXPANSION.md` §4/§7: two boots without
  association vs ~14 s for the canonical image under the identical procedure; root
  cause not established, candidates recorded). Per the brief's own rule the network
  baseline was NOT traded for controller support: the canonical image was restored and
  re-verified as the resting state, and live DS4 acceptance (pairing, d-pad,
  coexistence) is recorded NOT VERIFIED pending a Owner-supervised diagnosis of DS4-1.

### D6 — Tail game engine: one 1D lane model, six curated games

`games/tail_lane.h` — the reusable 1D gameplay surface: logical positions `0..N-1` over
the TailFramebuffer's length, deterministic (xorshift seeds, fixed-cadence GameHost
steering — the accepted LED-L009 engine is reused unchanged), difficulty-scaled, P1/P2
aware. Games (registry 5 → 11, all deterministic + host-golden-testable):

| Game | Players | Surface | Concept |
|---|---|---|---|
| `color_chain` | 1 | tail (+matrix HUD) | Zuma-like: colored segments advance; shoot/insert colors; 3+ same-color clears; combo scoring; lose when the chain reaches the endpoint |
| `light_race` | **1P/2P** | tail (+matrix scoreboard) | P1/P2 colored racers; repeated presses advance; first to the endpoint wins |
| `reaction_stop` | 1 | tail (+matrix HUD) | moving light; stop it inside the target zone; difficulty scales zone size/speed |
| `color_catch` | 1 | matrix target + tail pulses | matrix shows the target color; press when the matching pulse crosses the scoring point |
| `memory` | 1 | tail sequence (+matrix HUD) | Simon: reproduce the growing light sequence; matrix shows round/score/status |
| `tug_of_war` | **1P/2P** | tail (+matrix marker) | balance marker pulled toward the presser; first past the endpoint wins |

2P routing: P1 events and P2 events drive their own side; a solo session of a 2P-capable
game keeps P2 as AI/solo variant where sensible (light_race solo = time-trial vs clock;
tug_of_war solo = vs a deterministic drifting pull). No game degrades into an
unfinished demo; every game ships with determinism/bounds/scoring/difficulty/pause tests.

### D7 — Premium Mobile UI V3 (Owner-approved direction)

The served dashboard is rewritten to the approved V3 direction while keeping every
accepted boundary (pure same-origin API client, ASCII-only, self-contained PROGMEM, no
CDN/external resources, device authoritative):

- Six bottom-nav sections (Home, Text, Timer, Visuals, Games, Device).
- Compact design system: chips, small cards, icon buttons, bottom sheets + lightweight
  modals with backdrop, expandable Advanced areas, ~150–200 ms transitions, phone-first
  compact spacing; no wall-of-buttons, no permanent giant color grids (colors and styles
  open compact picker sheets), no permanently occupying controller pad (the virtual
  controller opens as a contextual sheet while a game/menu is active).
- Home: status, current mode + compact detail, uptime, compact brightness slider,
  quick-action row (Standby, Clear, Local menu, favorites); low-frequency controls move
  to Device.
- Text composer: persistent text field + Fixed/Marquee/Words selector + Show + live
  preview; color/bgColor and style open bottom sheets; context-sensitive controls only
  (duration for fixed, speed/loop/direction for marquee, dwell/loop for words); saved
  texts open as a sheet.
- Timer: large display, compact duration input + presets, one main START/PAUSE/RESUME,
  secondary Reset/Stop; device truth preserved (local interpolation re-synced on every
  event).
- Visuals: categories Ambient / Effects / Icons with small visual thumbnails, zone
  filter chips (Matrix / Tail / Both), expressions as compact mood chips; tail/both
  visuals live in the same system.
- Games: compact cards with difficulty + player-mode chips; controller sheet contextual.
- Device: identity/tail status, notifications, standby, visualizer, input test, advanced
  configuration; reconnection behavior preserved (WS exponential backoff + polling
  fallback + offline banner).
- Client preview gains a tail strip (compact 1D representation under the matrix canvas)
  mirroring tail-active programs — presentation only.

### D8 — Capability discovery extends; external adapters preserved

`GET /registry` gains animation zone metadata + per-game surface/players metadata;
`GET /state` gains `tail {enabled,length,reverse}` (and the animation target while
active); identity capabilities append `display.zones` (and `display.ds4` in the DS4
build). HUGO/HA adapters stay byte-identical and keep working (their embedded
vocabularies remain a valid subset — LED-L006 precedent); no raw-LED or framebuffer
surface exists anywhere. Battery/registry-aware clients update via discovery only.

### D9 — Compatibility and protected history

Everything is additive: frozen first-8 registry prefixes and their order untouched;
§5 core commands byte-unchanged; accepted debt (N-L003-1/N-L003-2/N-L004-1/F-L005-1/
N-L006-1/N-L006-2) untouched; the five D-L008 repairs and the LED-L009 render-gate work
stay verbatim; frozen baselines, pinned RWS, legacy repository untouched. LED-L009 stays
`verified`-unreviewed — this milestone neither accepts nor modifies its historical
record. Firmware version: `0.3.0-alpha.1`.

### D10 — Resource budget and honest limits

Measured before/after in the implementation record (flash, RAM, static/dynamic, frame
costs). The canonical env must stay safely within budget (target: ≤ ~80 % flash); the
DS4 env carries the BT stack's cost in its own image. If the classic ESP32 proves
genuinely constrained (measurement-backed), the blocker is recorded exactly and nothing
reliable is silently removed; ESP32-S3 migration seams stay clean.

## Consequences

- The working system is recoverable at all times: pre-L010 full-flash backup + the
  byte-reproducible `0f7a38de…` build + preserved historical backups.
- The tail ships disabled; the Owner performs the physical wiring AFTER the pin/count
  handoff, then bring-up resumes under the bounded sequence.
- DS4 lands as a separate env; if coexistence fails on the real device, the canonical
  env stays untouched and the blocker is recorded with evidence.
- Tail LED count stays UNVERIFIED (default 60 placeholder) until the Owner counts the
  physical rope; the config key makes it a one-line change, no rebuild.

## Verification anchors

Baseline battery at milestone start: C++ host 31073/0 (+ ASAN/UBSAN clean), node 33/0 +
boot 74/0, HA 107/0, HUGO 158/0; artifact `0f7a38de…` (942352 B) byte-identical local
rebuild, live on the device; worktree fingerprint 181 files `cd558645…`. All end-state
numbers are recorded in `docs/work/evidence/LED-L010-EXPANSION.md`.
