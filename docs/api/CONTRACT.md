# LED Wall Semantic API — Wire Contract (v1, implemented subset)

Status: historical LED-L003..LED-L010 contract plus the LED-L011 V4.1 candidate addendum at the end. Earlier implementation and limitation paragraphs describe their named milestones; the LED-L011 addendum states the current unreviewed software candidate. Authoritative semantics: frozen spec plus ADR-LED-L009, ADR-LED-L010 and the bounded ADR-LED-L011 architectural amendment. Hardware claims remain bound to their own evidence records.

## Transport

- HTTP (commands/queries): port `api.http_port` (default **80**). JSON bodies; response always `application/json`.
- WebSocket (events): port `api.ws_port` (default **81**), binary-protocol frames carrying JSON text.
- **`GET /`** serves the embedded dashboard (LED-L004 base, LED-L009 premium reconstruction): one self-contained `text/html` document (no external resources) that operates the device **exclusively as a same-origin client of the API documented below** (HTTP commands/queries + WS events; favorites in browser `localStorage`, never device state). `POST /` and every other undocumented path behave exactly as before (404 `unsupportedCapability`). The dashboard adds no device authority beyond any other LAN client. It deliberately merges scenes into the animations picker (§7: animations ARE usable as scenes) to avoid duplicate controls and therefore does not call `/display/scene/set` — that endpoint remains fully available to every other client.
- No network discovery in v1; configured endpoint (the LED-L009 `GET /registry` is an authenticated-by-LAN-position read-only endpoint on the same configured host, not a discovery protocol). No authentication in v1 — documented LAN-trust residual risk (§9).
- **Home Assistant** integrates via the external adapter (LED-L005, ADR-015) under `ha_adapter/` — a host-side client of exactly the contract below that bridges to MQTT; MQTT never enters the firmware. See `ha_adapter/README.md`.
- **HUGO** integrates via the external DeviceAdapter (LED-L006, ADR-LED-L006) under `hugo_adapter/` — a host-side client of exactly the contract below exposing a bounded, intent-only JSON-RPC tool surface over stdio (frozen spec §16: text, scene, animation, expression, brightness, clear, state — plus spectrum/color only when advertised); zero firmware changes. The accepted adapters predate the LED-L009 additive surface and keep working unchanged (the §5 core surface they use is byte-identical); registry growth beyond their embedded vocabularies is discoverable via `GET /registry`. See `hugo_adapter/README.md`.
- Client→server WS messages are ignored in v1; commands are HTTP-only.

## Commands (frozen §5 core — unchanged by LED-L009)

All command endpoints are `POST` with a JSON object body (`{}` allowed where no parameters exist). Unknown parameters and wrong types are rejected with `invalidParameter` — malformed commands mutate nothing (§11).

| Endpoint | Body | Semantics |
|---|---|---|
| `POST /display/text/show` | see the Text Studio table below | Persistent text baseMode; with `durationMs` a transient |
| `POST /display/text/clear` | `{}` | §11 CLEAR-2: text-as-base → `baseMode=idle`; text-as-transient → cancel transient, return to base; otherwise no-op. Never restores a historical stack |
| `POST /display/scene/set` | `{"scene": name, "durationMs"?: int(>0), "target"?: "matrix"\|"tail"\|"both" (LED-L010)}` | Selects a registered scene. The animation registry is usable as scenes (§7): the registry accepts all 36 animation names (the frozen 8 + the LED-L009 curated sets + the LED-L010 tail/combined set — each renders the same deterministic visual as `animation.set`). Unknown → `unknownScene` + available list. `target` semantics identical to `animation.set` below |
| `POST /display/animation/set` | `{"animation": name, "durationMs"?: int(>0), "target"?: "matrix"\|"tail"\|"both" (LED-L010)}` | Selects a registered animation. Registry (additive — the frozen §7 eight first, in order, unchanged): `corners, checker, border, row_sweep, bounce, wave, rain, plasma` + `smile, wink, heart, heart_pulse, eyes, loading, success, warning, arrows` (icon set) + `aurora, stars, rainbow_flow, fire, matrix, calm` (ambient set) + LED-L010 tail/combined set `comet, scanner, pulse, breathing, meteor, spark_trail, lightning, fire_flow, progress, countdown_progress, chase, color_wave, heartbeat` = 36 names. Unknown → `unknownAnimation`. **LED-L010 zone target:** optional `"target"` — `matrix` \| `tail` \| `both`; absent = the animation's default (matrix for every pre-L010 name and rainbow_flow; tail for the 13 new names). A target is valid only if the animation's registry metadata supports it (`tail`-only names reject matrix; only `rainbow_flow` + `pulse, breathing, lightning, progress, countdown_progress, chase, color_wave, heartbeat` accept `both`) — violations ⇒ `invalidParameter`. A tail-involving target while the tail zone is disabled (config `tail.enabled` = 0, the shipping default) ⇒ **`ledUnavailable`** (the reserved §9 code, activated for exactly this semantic). A tail-target program renders the matrix safe blank; a `both` program renders the matched matrix counterpart + the tail effect; combined mode semantics (timer progress, notification accent, expression tail wash, spectrum bass) are renderer-integrated per ADR-LED-L010 D3 |
| `POST /display/expression/set` | `{"expression": name}` | Persistent expression intent, realized internally through a data-driven mapping to the canonical renderers. Vocabulary (LED-L009, additive — the frozen §8 eight first, in order, unchanged): `idle, listening, thinking, speaking, happy, alert, error, sleeping` + `bored` (very slow dim plasma), `tired` (very slow cool wave), `focused` (calm cyan border pulse), `excited` (fast wide-spectrum wave) = 12 intents. Frozen examples unchanged: thinking → slow wave, alert → fast red border pulse, error → solid red border, sleeping → dim rest. No `durationMs`. Unknown → `unknownExpression` |
| `POST /display/clear` | `{}` | §11 CLEAR-1: cancel transient, blank framebuffer, `baseMode=idle`, no automatic animation. Always available (also cancels an active notification) |
| `POST /display/brightness/set` | `{"level": int(0..100)}` | Clamped by the device ceiling (§13/ADR-011). During the bounded Tail Setup point diagnostic, requests above 5 are rejected with `invalidParameter`; outside that diagnostic, high requests are clamped as before. Does NOT wake standby |

LED-L010 Tail Setup adds two narrow physical-count diagnostic commands:

| Endpoint | Body | Semantics |
|---|---|---|
| `POST /display/tail/point` | `{"index": int(0..199)}` | Requires the Tail zone enabled and `index < tail.length`; otherwise `ledUnavailable` or `invalidParameter`. Clears the current program and Matrix, fixes brightness at 5, and holds exactly one white Tail cell until OFF or another successful display command. The 200-position bound is a test limit, never a count claim. No color or framebuffer data is accepted |
| `POST /display/tail/off` | `{}` | Clears the diagnostic point and display. It does not change stored Tail configuration |

`durationMs`-qualified commands create the single transient (replace-not-queue; expiry returns to the baseMode; deadline arithmetic is wrap-safe across the uint32 millis() rollover for durations ≤ 2³¹−1 ms).

## Text Studio (`POST /display/text/show`, LED-L009)

| Field | Type / bounds | Semantics |
|---|---|---|
| `text` | `string(1..128)`, required | The message. The 128-character protocol cap is byte-based at the transport (§6); rendering width counts code points |
| `scroll` | `bool`, optional (legacy) | Heritage flag: `false`/absent → static (unless `mode` overrides); `true` → marquee in the heritage LEFT→RIGHT direction — byte-compatible with every LED-L007 client behavior and golden |
| `mode` | `"fixed"` \| `"marquee"` \| `"words"`, optional | Text Studio mode; when present it overrides `scroll`. `fixed` = static centered; `marquee` = continuous wrap, **RIGHT→LEFT by default** (the normal reading direction; `direction` overrides); `words` = word-by-word |
| `direction` | `"rtl"` \| `"ltr"`, optional | Marquee direction. `rtl` (default for an explicit marquee) = the text enters from the right edge and moves naturally toward the left, wrapping; `ltr` = the heritage left→right traversal |
| `speedMsPerPx` | `int(40..500)`, optional, default 140 | Marquee speed: milliseconds per pixel of travel (lower = faster) |
| `loop` | `bool`, optional, default `true` | `words`: loop forever vs one-shot. Marquee loops by construction |
| `wordDwellMs` | `int(200..60000)`, optional, default 2000 | `words`: display duration of each whitespace-separated word |
| `durationMs` | `int(>0)`, optional | Transient form (§11); otherwise the persistent base |
| `style` | `"solid"` \| `"color_bg"` \| `"rainbow_text"` \| `"rainbow_bg"`, optional, default `solid` | `solid`: black background + text color. `color_bg`: `bgColor` background + automatically readable foreground (white/black by perceived luminance). `rainbow_text`: per-column rainbow glyph pixels on black (hue follows the absolute screen column, drifting slowly). `rainbow_bg`: cycling rainbow background, white glyphs |
| `color` | palette name, optional, default `heritage_green` | Text color for `solid`/`color_bg`. Palette (13): `black, white, gray, red, orange, yellow, green, cyan, blue, purple, pink, warm_white, heritage_green` (`heritage_green` = the accepted LED-L007 text color) |
| `bgColor` | palette name, optional, default `black` | Background color for `color_bg` |

Width rule (§6, render-time): width = **code-point count × 6 px** (the heritage n×6 advance generalized to decoded code points). `fixed` text fitting the 30 px canvas (≤5 glyphs) renders statically centered; wider `fixed` text ⇒ ERROR `textTooWide` with no mutation (the client retries with `mode:"marquee"` or `scroll:true`). Marquee and words render any length. `words` one-shot shows each word for `wordDwellMs` then a stable blank and emits `text.done`; looping words wrap forever; a one-shot `words` program never reports `textTooWide`.

Charset (LED-L009 extended): `A–Z`, `a–z`, `0–9`, `space`, `- . ! , ? : ; ' " ( ) + = % / * # _ < >` and the Albanian letters `Ë ë Ç ç` (UTF-8 decoded). Every other code point renders as one 6 px space; the ack reports `skippedCharacters` counting unsupported **code points** (an invalid UTF-8 byte counts one). LED-L007 clients see identical behavior for the previously supported set apart from the counting unit (documented change; was byte-based).

## LED-L009 additive commands

All bounds and validations below are enforced by the firmware (`http_server.cpp` parsing, `command.cpp` dispatch); malformed input returns `invalidParameter` and mutates nothing.

| Endpoint | Body | Semantics |
|---|---|---|
| `POST /display/timer/set` | `{"durationMs": int(1..5999000)}` | Arms the device-side countdown. 5999000 = 99:59, the largest `MM:SS` value that fits the 30 px canvas. Out-of-range ⇒ `invalidParameter`, no mutation |
| `POST /display/timer/start` | `{}` | Engine `idle→running`, `paused→running` (resume), `finished→running` (restart); the display base becomes `timer` (wakes standby). Never armed ⇒ `invalidParameter` ("no countdown duration is set") |
| `POST /display/timer/pause` | `{}` | `running→paused` with the **exact remaining milliseconds frozen**; otherwise a no-op `{"ok":true}` |
| `POST /display/timer/resume` | `{}` | `paused→running`, continuing from the frozen remainder (never from the duration); otherwise a no-op `{"ok":true}` |
| `POST /display/timer/reset` | `{}` | Re-arm the stored duration (engine `idle`); the display stays wherever it is |
| `POST /display/timer/stop` | `{}` | Reset AND leave the timer display: if the base is `timer` it returns to `idle` |
| `POST /display/input` | `{"input": "up"\|"down"\|"left"\|"right"\|"confirm"\|"back"\|"start_pause"\|"home", "player"?: 1\|2 (LED-L010)}` | One normalized event into the unified input queue (phone as controller; same stream the physical controls and controllers feed). Unknown name ⇒ `invalidParameter`. **LED-L010:** optional `"player"` (1 = P1 default, 2 = P2) attributes the event to a player slot — games route by slot (duels), single-user consumers (menu/timer/standby) ignore it. Out-of-range player ⇒ `invalidParameter`. Queue full ⇒ `{"ok":true}` as a bounded no-op (drop counted) |
| `POST /display/menu/set` | `{"action": "enter"\|"exit"}` | `enter`: open the local pixel menu (base → `menu`; pauses a running game). `exit`: close (base → `idle` if it was `menu`; no stack). Missing/other action ⇒ `invalidParameter` |
| `POST /display/game/set` | `{"game": name, "players"?: 1\|2 (LED-L010)}` | Start a game in the `ready` phase (base → `game`; `confirm`/`start_pause` starts). Registry (additive): matrix classics `pong, snake, breakout, reaction, dodger` + LED-L010 tail games `color_chain, light_race, reaction_stop, color_catch, memory, tug_of_war` = 11 names. Unknown ⇒ `unknownGame` + available list. **LED-L010:** optional `"players"` selects the player mode, applied at reset exactly like difficulty; `2` is valid only for games whose registry metadata supports it (`pong`, `light_race`, `tug_of_war`) — otherwise `invalidParameter`. `/state.game.players` reports the engine's mode |
| `POST /display/game/command` | `{"action": "pause"\|"resume"\|"restart"\|"exit"}` | Game lifecycle: pause/resume affect the `running`/`paused` phases; `restart` resets to `ready`; `exit` tears the game down and returns the base to `idle`. Missing/unknown action ⇒ `invalidParameter` |
| `POST /display/game/difficulty` (V-L009-3, LED-L009 repair cycle 2) | `{"difficulty": "easy"\|"normal"\|"hard"}` | Engine-enforced difficulty. The level persists in the runtime across game sessions (default `easy`); changing it during a game applies at the next start/restart. Games derive their timing from it at reset — the phone never simulates speed. Unknown/missing ⇒ `invalidParameter` |
| `POST /display/visualizer/start` | `{}` | Base → `spectrum`: render the external band feed |
| `POST /display/visualizer/stop` | `{}` | Clear the feed and return the base to `idle` if it was `spectrum` |
| `POST /display/visualizer/data` | `{"bands": [30 ints, each 0..255]}` | Semantic external spectrum input — source-agnostic (phone, PC, HUGO, HA, a future microphone bridge). Exactly 30 values, each 0..255; wrong shape ⇒ `invalidParameter`. Rate limit: one accepted feed per 25 ms (violation ⇒ `invalidParameter`, no mutation). 1500 ms of silence ⇒ inactive with a deterministic decay (~2000 units/s) to a blank, fail-safe display |
| `POST /display/notify` | `{"text": string(1..128), "durationMs"?: int(1..300000), "priority"?: "normal"\|"high"}` | Semantic notification: a `notification` transient above the current base; expiry automatically restores it. `durationMs` defaults to 5000. `high` adds pulsing-red-border styling (priority is presentation only — replacement stays replace-not-queue). Notification text wider than the canvas auto-marquees RIGHT→LEFT (never `textTooWide`). Empty text / bad duration / bad priority ⇒ `invalidParameter` |
| `POST /display/standby/set` | `{"on": bool}` | Semantic display-off: blank panel, no animation work, network and config preserved, no reboot required. Any content command wakes (replaces the base); `POST /display/brightness/set` deliberately does NOT; physical `confirm`/`back`/`start_pause` wake. Missing `on` ⇒ `invalidParameter` |

**Countdown/timer model (device truth).** The engine runs on the single monotonic device clock and is fully phone-disconnect independent — closing every client never pauses or resets it. States: `idle` (armed) → `running` ⇄ `paused` → `finished` (stable at zero; the panel shows `0.000` with a pulsing red border). `remainingMs` in `/state` is exact at serialization time; the LED form is `MM:SS` ≥ 60 s, `SS` ≥ 10 s, `S.mmm` below 10 s. Physical one-button model (identical semantics for the API `confirm`/`start_pause` inputs on the timer screen): press starts, press again pauses, press again resumes; `back` leaves the timer display (the engine keeps counting); `left`/`right` re-arm ±5 s while idle.

**Game difficulty (V-L009-3).** Engine-enforced per game at reset: Pong ball 1/2/3 px per step with AI tracking every 3rd/2nd/1st step and rally-speedup caps 2/4/5 (Easy/Normal/Hard); Snake a move every 400/300/200 ms with a speedup floor of 3/2/1; Breakout 1/2/3 ball cells per step with a 7/5/5-wide paddle; Reaction a 12/10/7 s visible countdown window with exact-millisecond stop precision in every mode (difficulty never distorts the clock); Dodger obstacles every 300/200/100 ms with speedup floors 2/1/1 and spawn bases 4/3/1. Default `easy`, persisted in the runtime, applied at the next start/restart of the active game.

**Local menu model.** Root sections `TIMER, ANIMS, EXPRS, GAMES, BRI <level>, STBY, EXIT` (position dots on the bottom row; selection marker blinks). `up`/`down` navigate, `confirm` enters/selects, `back` steps back, `home` exits from anywhere. List selections apply through the same semantic commands as the API (animation/expression/game select, brightness step, standby toggle, timer open). Long registry names auto-marquee inside the menu. Entering the menu pauses a running game; exiting lands on `idle` (§11 no-stack).

## Queries

- `GET /identity` (§10): `deviceId` (`led-wall-<efuse24>`), `deviceType` (`visual_display_v1`), `role` (`display`), `protocolVersion` (1), `fwVersion` (semver; `0.3.0-alpha.1` at the LED-L010 implementation), `capabilities[]`, `matrix {width:30, height:10}`.
  - `capabilities[]` (17, LED-L010): the frozen §5 core set first, byte-unchanged — `display.text.show, display.text.clear, display.scene.set, display.animation.set, display.expression.set, display.clear, display.brightness.set, display.state.read` — followed by the implemented additive set `display.timer, display.games, display.menu, display.input, display.notify, display.standby, display.visualizer, text.styles` (LED-L009) and `display.zones` (LED-L010). Conditional capabilities appear only when implemented/confirmed: `display.spectrum.start/stop` still require confirmed microphone hardware (omitted — **no microphone exists; the visualizer is external input, not a mic**); `display.ds4` is advertised ONLY by the `esp32_eq_ds4` build (the canonical image never claims controller support); `color`, `paint.diagnostic` not implemented — omitted.
- `GET /registry` (LED-L009 + LED-L010): current registries for discovery — `{"animations": [36 names], "expressions": [12], "games": [11], "inputs": [8], "textStyles": [4], "palette": [13], "animationZones": {name: ["matrix"|"tail"|"both"...], ...36}, "gameMeta": {name: {"zone": "matrix"|"tail"|"both", "players": 1|2}, ...11}}`. The first 8 animation and first 8 expression names are the frozen sets in their original order; `animationZones` + `gameMeta` are the LED-L010 discovery surfaces (ADR-LED-L010 D8) — the accepted HA/HUGO adapters keep working unchanged (their embedded vocabularies remain a valid subset).
- `GET /state` (§4 + LED-L009 + LED-L010): `available`, `deviceId`, `deviceType`, `protocolVersion`, `fwVersion`, `baseMode`, `currentMode`, `activeTransient` (`{kind, remainingMs}` or null), `activeScene`/`activeAnimation`/`activeExpression` (base program name when applicable, else null), `text` (when text applies; transient text overrides base text and expiry falls back without mutation), `brightness {current, max}`, `spectrum {available, active}` (reserved for confirmed mic hardware — `available` stays `false`), `matrix {width, height}`, `clients {count}`, `warnings[]`, `uptimeSeconds`.
  - `text {current, scrolling, mode, rtl, loop, wordDwellMs, style, color, bgColor}` — `scrolling` is the legacy projection (`true` iff mode is `marquee`, including the heritage `scroll:true` mapping); `mode` is the Text Studio mode; `style`/`color`/`bgColor` are the semantic names.
  - `timer {state: idle|running|paused|finished, durationMs, remainingMs}` — always present.
  - `menu {active}`.
  - `game {active, name, phase: ready|running|paused|over, score, difficulty: easy|normal|hard, players: 1|2 (LED-L010)}` — `difficulty` and `players` are engine-enforced levels (V-L009-3 / LED-L010), not per-session UI state.
  - `standby {on}`.
  - `notification {active, high, text?}` — `text` present while a notification is active.
  - `visualizer {active}`.
  - `tail {enabled, length, reverse, scanIndex, setupIndex}` (LED-L010) — the configured tail zone (dotted config `tail.*`; while Tail Setup is active, `length` may be a temporary scan bound, not the measured count). The default 60 is a documented PLACEHOLDER. `scanIndex` is the zero-based logical cell currently lit by the moving one-hot `scanner` when it is the current Tail-target scene/animation; it is null otherwise. `setupIndex` is the steady point selected by the bounded Tail Setup diagnostic, null when OFF. Both are read-only state. `animationTarget` (LED-L010): `"matrix"|"tail"|"both"` while a scene/animation program applies (any slot), else null.
  - `ds4 {supported, enabled, state, players, battery: [p1, p2]}` (LED-L010) — `supported` is true only in the `esp32_eq_ds4` build; `state`: `unsupported|unpaired|pairing|connected|disconnected|reconnecting`; `players` = connected controllers; battery 0..100 per slot (0 = unknown).
  - Mode vocabulary for `baseMode`/`currentMode`: the seven §4 kinds `idle, text, scene, animation, expression, spectrum, paint` plus the LED-L009 kinds `menu, timer, game, notification, standby`.

## Responses

- Success: `200 {"ok": true}` — `POST /display/text/show` additionally reports the skipped-character count: `200 {"ok": true, "skippedCharacters": <n>}`.
- Typed error (§9): `400 {"ok": false, "error": {"code": "<code>", "message": "...", "available"?: "a, b, c"}}`.
  Codes: `textTooWide` (fixed text wider than 30 px), `unknownScene`, `unknownAnimation`, `unknownExpression`, `unknownGame` (LED-L009), `invalidParameter`, `unsupportedCapability` (also returned for unknown paths), `ledUnavailable` (LED-L010: tail-target request while the tail is disabled), `internalError` (also returned if an LED-L009 command reaches the dispatcher with device services unavailable — a fail-closed guard that cannot occur in the shipped runtime).
- HTTP status maps: 200 ok; 400 typed error; 404 unknown path.

## Events (WebSocket, §9)

JSON envelopes; state payloads mirror `GET /state`:

- `{"event": "state.changed", "state": {…}}` — any accepted mutating command or expiry (timer/menu/game/input/visualizer-feed events per the table above; a bare visualizer data feed intentionally emits nothing — it is not §4 state).
- `{"event": "mode.changed", "currentMode": "…"}` — current mode kind changed.
- `{"event": "transient.expired"}` — a transient reached its deadline.
- `{"event": "text.done"}` — render-completion event (LED-L007, LED-L009 wording): the active text program has completed one full display — fixed text once its first frame has actually been shown; a marquee after one full wrap traversal (`speedMsPerPx` × span); a one-shot word program after its last word's dwell (looping programs never complete). Emitted once per program activation; the base and transient slots are tracked independently. A transient replaced or cancelled before completion emits nothing.
- `{"event": "timer.done"}` (LED-L009) — the device-side countdown reached zero (state `finished`).

Broadcasting is bounded: pending events coalesce into at most one state broadcast per main-loop iteration; slow/dead clients are dropped (§9 heritage hardening).

## Configuration (§18)

NVS namespace `ledwall`, storage keys in the product's dotted config vocabulary (the LED-L008 D-L008-2 repair — underscore spellings were rejected by the config applier and could never apply): `orientation.flip_odd_rows`, `orientation.flip_y`, `brightness.default_level`, `brightness.ceiling`, `boot.base_mode`, `wifi.ssid`, `wifi.password` (secret — never reported), `api.http_port`, `api.ws_port`, `input.enabled` (LED-L009, default `0`), and the LED-L010 keys `tail.enabled` (default `0` — the tail ships disabled until the Owner wires the rope), `tail.length` (1..300, default 60 — documented PLACEHOLDER until the Owner counts the physical rope; software never infers LEDs per metre), `tail.reverse` (default `0`), `ds4.enabled` (default `1`; consumed only by the `esp32_eq_ds4` build, harmlessly ignored elsewhere).

- Missing keys keep spec defaults; unknown keys and invalid values are ignored with a warning that surfaces in `warnings[]`.
- There is no network config-write endpoint. `tools/nvs_provision.py` adds only allow-listed non-secret values to a local NVS image, writes an exclusive mode-`0600` pre-change backup, and refuses to overwrite an existing backup; WiFi credentials use the separate LED-L008 provisioning procedure. The tool always redacts WiFi secrets, including when `list --show-values` is used. Writing an NVS image to hardware is a separate, explicitly authorized operation.
- `boot.base_mode` accepts any registered mode name; new modes render fail-closed (safe blank) if booted into without their subsystem state.
- `input.enabled` enables the physical GPIO input provider. It ships **off**: the joystick/button shield pinout exists in no repository evidence; the candidate pin map (`firmware/src/input/input_pins.h`) must be confirmed by the Owner before enabling. With the provider off, no input pins are touched.
- **LED-L010 GPIO audit (ADR-LED-L010 D2):** MATRIX data = GPIO13 (canonical, unchanged); **TAIL data = GPIO27** (audit-locked; the candidate `left` input moved to GPIO4); GPIO16/17 RESERVED for the future wired P1 RP2040 node (UART2); the candidate input map now uses 4/14/19 in place of 27/16/17 and stays disabled by default. The pin lives only in `firmware/include/device_constants.h`.

## LED-L010 expansion summary (ADR-LED-L010)

- **Zones:** two logical display surfaces — the 30×10 matrix and the 1D tail (1..300 bounded logical cells; physical reverse applied in the driver). Matrix behavior is unchanged when the tail is disabled (shipping default); tail-target commands fail `ledUnavailable` while disabled. Tail data pin GPIO27; separate from the matrix by design (independent semantic control).
- **Tail/combined animations:** the 13 appended registry names (§ animation.set table) plus `rainbow_flow` gaining tail/both. Combined semantics integrate into the accepted modes: the timer drives a tail progress drain (green→orange→red, pulsing at finished); high-priority notifications drive a red tail chase (normal: a calm static accent); expressions wash the tail in their semantic color (alert = red chase, error = solid red, rest = dark, wave/plasma = slow wash); the visualizer adds a bass-band rope wash; standby blanks the tail.
- **P1/P2:** events carry a player slot end-to-end (wire `player`, queue, engine). Duels: Pong 2P (P1 left / P2 right, AI off), `light_race`, `tug_of_war`. Engine mode via `game.set players`, reported in `/state.game.players`, applies at the next start/restart.
- **DS4 (esp32_eq_ds4 build only):** Bluepad32 prebuilt core 4.1.0 (`platform_packages` swap + `huge_app` partition layout — the BT-Classic stack costs ~630 KB flash, measured; the default 1.25 MB app partition overflows at 120.9 %). Mapping: d-pad → up/down/left/right (auto-repeat 400 ms delay / 150 ms period); Cross→confirm; Circle→back; Square→left; Triangle→right; OPTIONS→start_pause; PS→home. Slots: first connect = P1, second = P2. The canonical `esp32_eq` image contains zero Bluetooth code. The wired-controller seam (future RP2040 nodes over UART; P1 on GPIO16/17) is reserved as a pure, host-tested packet codec (`firmware/src/input/wired_controller.h`).
- **Premium Mobile UI V3:** the served dashboard (51,947 B PROGMEM at the LED-L010 implementation) is rebuilt to the Owner-approved direction — six bottom-nav sections, compact chips/cards, bottom sheets (text color, background, style, saved texts, controller), contextual controller pad (floating button while a game/menu is active; P1/P2 slots), Visuals categories (Ambient/Effects/Icons/Tail) with a zone filter, per-game player chips, a presentation-only tail preview strip, and Device-side tail/DS4 status. Boundaries unchanged: same-origin pure API client, ASCII-only, no external resources, device authoritative.

## Compatibility with historical LED-L007 behavior

- The §5 core capability list, the seven §4 mode kinds and their wire names, the §9 typed-error vocabulary, the §11 CLEAR/transient semantics, and the transport defaults are unchanged.
- `scroll:true`/`scroll:false` on `text.show` map exactly as before (static vs heritage LTR marquee); LED-L007 golden behavior is preserved and regression-pinned.
- The frozen first-8 registry names (animations and expressions) keep their original order; additive names are appended only.
- `skippedCharacters` counting changed from bytes to code points (one unsupported code point = one 6 px space). This is the single documented wire-visible change to a historical response field (resolves LED-L007 observation O-L007-R5); for previously-supported ASCII text the value is unchanged.
- The HA/HUGO adapters compiled against the LED-L003/007 contract continue to work unchanged (verified: both suites green, sources diff-free); their embedded vocabularies remain a valid subset.

## Current limitations (honest)

- **Device-side vs unverified:** routes, validation bounds, state fields and events above are implemented and host-tested (`-Wall -Wextra -Werror`, 31073-check battery plus a boot-execution suite running the exact served dashboard script) and the LED-L009 surface was instrument-verified live on the physical device (identity/registry/state, Text Studio acks, timer exact-ms lifecycle, game/menu/input flows, standby wake rules, notification restore, visualizer feed, WS `state.changed`, reboot recovery, dashboard boot/navigation in a real browser). **Eyes-on visual correctness of every new visual on the panel is Owner-supervised and not fully swept; the bounded soak and ALL PRODUCTION electrical gates remain NOT VERIFIED.**
- `deviceId` has been observed as `""` on the physical device since the LED-L008 campaign (host tests pass with the identifier logic; the on-device divergence is carried as open observation O-L008-1/O-L009-1 — non-blocking, unresolved).
- No microphone, no FFT, no ADC capture exists anywhere in the firmware (frozen spec §14/ADR-012 honored): `spectrum.available` stays `false` and `display.spectrum.*` stays unadvertised. The visualizer is a generic external input; future sources are indistinguishable by contract.
- Sound cues (`timer.done`, game start/over, notification, button) are semantic seams into a no-op sink — no speaker hardware exists, nothing is advertised.
- The DualShock 4 / Bluetooth controller is deliberately NOT implemented (resource/stability budget on the current `esp32dev` board; ADR-LED-L009 D11). `/display/input` and the GPIO provider are the controller-agnostic migration seam; no game requires a specific controller.
- Heritage palette note: renderers express the heritage CHSV colors through a deterministic integer HSV→RGB conversion so host golden frames and device output are identical; exact RGB output may differ from the heritage panel's FastLED rainbow rendering (internal, §3).
- Scroll/word timing uses program-relative monotonic ms (wrap-safe across the uint32 rollover); a marquee is a continuous wrap and reports `text.done` after each first full traversal per activation.
- `spectrum.*` styles and the paint capability stay unimplemented and unadvertised; requesting them is impossible through the API (no such capability), and the corresponding mode kinds render safe blank defensively.
- The dashboard discovers the WS endpoint by the documented default port 81 and falls back to `GET /state` polling (5 s) when unavailable; a non-default `api.ws_port` is not discoverable via the API in v1.

## LED-L011 V4.1 software candidate addendum (2026-10-15)

This section describes the uncommitted isolated candidate governed by
`docs/decisions/ADR-LED-L011-V4_1-ARCHITECTURE-AMENDMENT.md`. It supersedes the
LED-L010-only registry counts and the historical statement that paint is
unimplemented. Existing names, routes and the first accepted registry entries
retain their order. It does not claim hardware verification or acceptance.

### Two-zone rendering and product identity

- Matrix is 30×10 row-major. Tail has logical `t=0..149` from the matrix
  anchor toward the free end; physical reverse stays in the driver. The device
  renders one semantic foreground program into both bounded logical buffers.
  Each render assigns the same monotonically incremented nonzero `frameId`
  to Matrix and Tail buffers, then stages both before one `show()` call.
  This is a software composition identity, not proof of simultaneous physical
  latch or an API-exposed frame counter.
  E01–E10 are `fireworks`, `meteor_impact`, `energy_bloom`, `rainbow_river`,
  `heartbeat` (the existing ID with a coordinated Both path), `storm`,
  `lava_fountain`, `rain_drain`, `aurora_current`, `starstream`. The nine new
  names are appended to the animation registry. All ten require `target:both`
  for their cross-zone form; the existing Heartbeat target behavior remains.
- The driver applies a software current estimate after composition. It uses
  20 mA per full RGB channel scaled by the actual brightness byte, with
  default estimate budgets 800 mA Matrix, 200 mA Tail, 1000 mA combined.
  `estimated_before_ma` is an estimate, never a current measurement. Device
  LED type, supply, cabling, thermal behavior and physical timing are
  NOT_VERIFIED. The existing brightness ceiling of 40 remains.
- The firmware registry exposes 14 game IDs in 12 product families: five
  original matrix IDs, six LED-L010 Tail IDs, `tail_tennis`, `light_quest`,
  and the `quick_draw` Reaction variant. `reaction` remains the legacy matrix
  stop-at-zero game; `reaction_stop` remains the Tail stop variant. The
  2P-capable IDs are Pong, Tail Tennis, Light Race, Reaction Stop, Quick Draw,
  Tug of War, Memory and Color Catch. Light Race and Quick Draw require 2P.
  Tail games require at least 12 configured cells except Color Chain, which
  requires 30 grouped game cells. The 150-LED product form maps each Color
  Chain cell to five LEDs. `/registry.gameMeta` reports `zone`, `players`,
  `minPlayers` and `minLane`.

### Added local API

| Route | Request | Response and rule |
|---|---|---|
| `POST /display/pixel/session` | `{}` | `{ok,session,revision:0}`; a new session invalidates older revisions. |
| `POST /display/pixel/frame` | `{session:uint32,revision:uint32,pixels:string,live?:bool}` | Exactly 2700 hex characters, six per RGB cell, 300 Matrix then 150 Tail. Full validation precedes apply. Success ACK includes the same session and revision plus `applied:true`. Stale session/revision, invalid frame and disabled or non-150 Tail fail without changing the canvas. Live submissions are capped at 10/s in the core. |
| `GET /display/pixel/projects` | none | `{ok,available,capacity:4,projects:[names]}`. |
| `POST /display/pixel/project/save` | `{schemaVersion:1,name,pixels}` | Saves a complete validated frame without displaying it; ACK `{saved:name}`. Name is 1–24 ASCII letters/digits/`_`/`-`. |
| `POST /display/pixel/project/load` | `{name}` | Returns `{schemaVersion:1,name,pixels}` to the browser draft; does not display it. |
| `POST /display/party/play` | `{preset}` (Game Night may include registered `game`) or `{name,loop,steps:[...]}` | Presets: Calm, Welcome, Birthday, Celebration, Game Night, Countdown Finale. Custom steps: `text`, `effect`, `wait`; each has `durationMs` 100..600000, at most eight steps. Device owns the timeline after ACK. An active Game, Menu or Standby rejects Play. |
| `POST /display/party/stop` | `{}` | Stops an active timeline and clears its foreground. |
| `GET /display/party/projects` | none | `{ok,available,capacity:4,projects:[names]}`. |
| `POST /display/party/project/save` | `{schemaVersion:1,scene:{name,loop,steps}}` | Validates and saves a scene recipe without playing it; ACK `{saved:name}`. |
| `POST /display/party/project/load` | `{name}` | Returns `{schemaVersion:1,scene}` into the UI draft without playing it. |
| `POST /display/input/session` | `{player:1\|2}` | Claims an unowned phone slot; returns `session` token and `sequence:0`; busy slot returns `playerBusy`. |
| `POST /display/input/event` | `{player,session,sequence,input,state:"press"\|"release"}` | Only a fresh sequence for that slot is accepted. One held press produces one gameplay event; release clears it. Queue full and stale/wrong sessions fail closed. An idle session expires after 15 s. |
| `POST /display/input/heartbeat` | `{player,session,sequence}` | Fresh sequence renews the 15 s lease without generating game input; stale/wrong sessions fail closed. The served phone UI sends this every 5 s while visible and claimed. |
| `POST /display/input/release` | `{player,session}` | Releases ownership and all held buttons. A claimed phone slot excludes legacy API, DS4 and GPIO events for that slot. Release or expiry discards queued phone events for that slot and pauses a running game; reconnect requires an explicit Resume without resetting the board. |
| `POST /display/sound/mute` | `{muted:bool}` | Updates the bounded cue scheduler. The physical sink is disabled. |

`GET /state.game` retains `score` for legacy readers and adds `scoreP1`,
`scoreP2`, `winner` (`none|p1|p2|draw`), `highScore` and the existing
`difficulty`/`players`. The high score is written to a separate NVS key by
rules version, game ID, difficulty and player mode only when a completed
result improves it. A 2P key records the better player result without slot
bias. This NVS behavior is software/build verified only. `GET /state.sound`
reports `muted` and `physicalOutputAvailable:false`.

Text Show adds `tailMode:off|solid|rainbow_flow|pulse|progress|text_synced|custom`
and an independent palette `tailColor`; `custom` uses the last applied Pixel
Canvas Tail. Progress requires a real duration or a marquee/word cycle.
Timer completion triggers Countdown Finale only if Timer still owns the
foreground and Tail is enabled; otherwise the existing `timer.done` event
and finished state stand without foreground replacement. Browser refresh
never changes the device timer.

Pixel and scene device storage uses the existing SPIFFS partition only when
`SPIFFS.begin(false)` succeeds; no format or partition change occurs. The
canonical build reports storage unavailable if it cannot mount, and the DS4
`huge_app` build reports unavailable because it has no SPIFFS partition. The
store uses a bounded temporary file, backup rename and recovery on load; a
full store returns an error. Browser Save remains separate local storage with
Export for Pixel data. Neither browser storage nor the software rename
protocol is a proven power-loss backup. Physical read/write and fault
injection are NOT_VERIFIED.

The served dashboard retains Home, Text, Timer, Visuals, Games and Device.
Pixel Studio is a fullscreen workspace opened from Home/Text/Visuals. Save,
Send and Live are separate actions; Live defaults off, throttles at 100 ms
and never replays automatically after reconnect. Party recipes can be saved
locally or explicitly on the device. Cue scheduling is nonblocking, bounded
and globally muteable; no GPIO or piezo output is enabled. DS4 radio
coexistence, RP2040 hardware, current draw and eyes-on visuals require their
separate hardware gates.

## LED-L014 DS4 current-build overlay (2026-10-17)

The historical LED-L010 `huge_app` resource note above describes an earlier
build. The current `esp32_eq_ds4` build uses `partitions_existing_4m.csv`, an
exact description of the real 4 MiB device layout: app0/app1 each 1,310,720 B,
SPIFFS retained. The repaired DS4 firmware image fits app0 exactly and is installed. Live
Wi-Fi/UI/Bluetooth-host readiness is verified; controller pairing and gameplay
remain NOT_RUN. A build or capability alone never proves physical input.

In the DS4 build, `GET /state` includes `ds4.ready` (Bluetooth host initialized),
`ds4.assignedMask` (bit 0 P1, bit 1 P2) and monotonic `ds4.inputEvents`.
`POST /display/controller/pair` enables new Bluepad32 connections when ready;
otherwise it returns 409 `notReady`. `POST /display/controller/assign` takes
`{"player":1}` or `{"player":2}` and returns 409 `playerBusy` if the phone
owns the requested slot or the DS4 cannot move. A phone session claim returns
409 `playerBusy` when DS4 owns that slot. The DS4 d-pad uses Bluepad32's
up/down/right/left bitmask (1/2/4/8); the left stick has a deadzone and
hysteresis. Cross→confirm, Circle→back, Square→left, Triangle→right,
OPTIONS→start_pause, SHARE→home; PS is reserved. Bluetooth starts only after
Wi-Fi has been stable for three seconds when Wi-Fi is configured. These are
implementation contracts; real device/physical validation remains NOT_RUN.
