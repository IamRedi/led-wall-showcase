# LED WALL V1 SPECIFICATION — FROZEN v1.0

Status: `FROZEN FOR IMPLEMENTATION`
Scope: `LED Wall v1 only`
Supersedes: `LED WALL V1 ARCHITECTURE BASELINE`
Implementation authorization: `NOT YET AUTHORIZED`

Self-contained authoritative specification. Source: LED Wall Architecture Baseline + the LED Wall Panel Discovery Audit (deployed firmware `~/Documents/PlatformIO/Projects/PLUTO EQ ACCESSORY`, env `esp32_eq`, USER-VERIFIED / WORKING evidence). Three locked refinements (expression = V1 core; baseMode + single transient; CLEAR semantics) are integrated throughout.

---

## 1. Product definition

LED Wall v1 is **one standalone semantic visual display device**: an ESP32-driven 30×10 WS2812 matrix (300 LEDs, 10 physical strips) that renders text, scenes, animations, expressions, optional local-mic spectrum, and diagnostic paint, controlled exclusively through one public semantic API by HUGO, LOOI, Home Assistant, manual dashboards, and optionally Piano Core via an external adapter.

It **owns**: physical LED rendering, matrix mapping, frame timing, animations, text/font rendering, brightness (with locally enforced ceiling), the mode/state model, the optional microphone/spectrum mode, configuration and identity.

It **does not own**: HUGO/LOOI reasoning, Piano Core state, HA integration logic, other devices' state, cloud services, notification queuing, any multi-client ownership machinery.

**Relationship to HUGO/LOOI:** future `DeviceAdapter` targets; they request visual *intent* (text, scene, expression, notification, brightness, clear), never frame logic or renderer internals. **Relationship to Piano Core:** fully independent device (Piano Core ADR-012 unchanged); Piano Core may integrate later only through LED Wall's public API via an external adapter; the hard-coded Pi :8766 dependency is not product architecture. **Relationship to Home Assistant:** HA is an external adapter client of the same API.

## 2. Hardware contract

### VERIFIED (repository evidence — unchanged)
| Item | Value |
|---|---|
| Board family | ESP32 devkit (`esp32dev`), Arduino framework |
| LED type | WS2812 (FastLED, GRB) |
| LED count | 300 |
| Matrix | 30 wide × 10 high |
| Physical strips/rows | 10 rows of 30 |
| Data pin | GPIO13 |
| Microphone heritage | analog ADC GPIO34, 9 kHz, 256-sample FFT |
| Brightness heritage | 40 |
| Orientation | serpentine, odd rows reversed (`EQ_FLIP_ODD_ROWS 1`), runtime flip flags |
| Render cadence | dirty-flag + 50 ms floor ≈ 20 FPS max |
| Boot power heritage | brownout detector disabled |

### CONFIGURABLE
Semantic brightness + physical ceiling; scroll speed; expression→effect mapping table; scene/animation registry; orientation flags; WiFi credentials; device identity; boot default.

### PHYSICAL VERIFICATION REQUIRED
Exact devkit model; PSU voltage/current for 300 LEDs; shared ground; wall-mount orientation vs firmware flags; microphone hardware presence/wiring; thermal behavior; currently flashed firmware identity; safe maximum brightness; panel dimensions/mounting. (§22)

## 3. Public/internal boundary

| PUBLIC / STABLE V1 | INTERNAL / REPLACEABLE |
|---|---|
| Identity (`deviceId`, `deviceType`, role), protocol/firmware version, capabilities, matrix logical size | FastLED, GPIO13, WS2812, GRB order |
| Capability names + parameters/results (§5) | Serpentine index math, flip flags |
| State/mode model fields (§4) | ESP32/Arduino framework details, brownout handling |
| Command/query semantics, event semantics, error vocabulary (§9) | Animation/scene renderer implementations, registry bindings |
| Text contract semantics (§6) | Font bitmap implementation (5×7 glyph tables), glyph set |
| Expression vocabulary + intent semantics (§8) | Expression→effect/palette mappings |
| Arbitration/transient semantics (§10–§11) | Transient timer mechanics, frame scheduling internals |
| Brightness semantics 0–100 + existence of a device ceiling | Brightness scaling, dithering, physical drive details |
| `matrix {width,height}` logical canvas | Physical strip order, PSU/wiring, thermals |

Rule: replacing any right-column item never requires a public contract change.

## 4. State and mode model

### PUBLIC STATE
`available`; `deviceId`, `deviceType`, `protocolVersion`, `fwVersion`;
- `baseMode` — the persistent visual program (one of: `idle`, `text`, `scene`, `animation`, `expression`, `spectrum`, `paint`);
- `currentMode` — what is rendering now (`baseMode`, or the active transient's kind);
- `activeTransient {kind, remainingMs}` or `null` (at most one, §11);
- `activeScene` / `activeAnimation` / `activeExpression` (semantic names, whichever applies);
- `text {current, scrolling}` when text applies;
- `brightness {current, max}`;
- `spectrum {available, active}`;
- `matrix {width:30, height:10}`;
- `clients {count}`; `warnings[]`; `uptimeSeconds`.

### INTERNAL STATE
Frame buffer(s); render dirty-flag/throttle timers; scroll phase/offset; animation tick state; expression→effect mapping table; FFT buffers/band levels; paint buffer; transient deadline timer (exactly one); WiFi/WS session internals; config cache; orientation flags.

### DERIVED/UI-ONLY
The rendered frame; link-status corner indicator (legacy, reclassified derived); HA-derived sensors.

Visual state is not persisted; boot lands in the configured safe default (§12/§18).

## 5. Semantic capability contract

### V1 CORE (frozen)
| Capability | Parameters | Semantics |
|---|---|---|
| `display.text.show` | `{text, scroll?, speedMsPerPx?, colorOrHue?, durationMs?}` | persistent text baseMode, or transient if `durationMs` present (§6, §11) |
| `display.text.clear` | – | §11 CLEAR-2 semantics |
| `display.scene.set` | `{scene, durationMs?}` | select registered scene |
| `display.animation.set` | `{animation, speed?, durationMs?}` | select registered animation |
| `display.expression.set` | `{expression}` | request expression intent (§8); persistent |
| `display.clear` | – | §11 CLEAR-1 semantics; always works, any client |
| `display.brightness.set` | `{level: 0..100}` | clamped to device ceiling |
| `display.state.read` | – | full public state snapshot |

### CONDITIONAL (advertised in `capabilities[]` only if implemented/hardware-confirmed)
- `display.spectrum.start {style?, source: local_mic}` / `display.spectrum.stop` — local mic FFT mode (§14).
- `display.color.set {hue/scheme}` — global accent scheme for text/scenes.
- `display.paint.pixel {x,y,color}` / `display.paint.frame {pixels[]}` — **diagnostic-only** exception to the no-raw-coordinates rule, flagged `paint.diagnostic`; never used by normal clients.

No raw FastLED/GPIO/LED-index controls exist as normal capabilities. Registry names are the only scene/animation handles; legacy function names are never API names.

## 6. Text contract

- **Font heritage:** 5×7 bitmap font on a 30×10 canvas — exact font implementation is INTERNAL; the contract guarantees only: glyph-based text, the supported character set (A–Z, 0–9, space, basic punctuation per the glyph table), and explicit reporting of unsupported characters.
- **Static text:** where the rendered width fits 30 px (≤6 glyphs incl. spacing), render statically; client may force `scroll:true`.
- **Scrolling:** continuous horizontal wrap (heritage direction/speed model); `speedMsPerPx` bounded 40–500, default heritage 140.
- **Overflow rule:** `scroll:false` + text wider than the canvas ⇒ ERROR `textTooWide` (client retries with `scroll:true`); scrolling text has no practical length limit.
- **Message size:** ≤128 characters (protocol cap; legacy buffer was 48).
- **Unsupported characters:** rendered as space; ack reports `skippedCharacters: n`.
- **Brightness/color:** text renders at current brightness; color via optional parameter or the global accent scheme; default heritage palette.
- **Duration:** `durationMs` makes the text a transient (§11); without it, text is the persistent baseMode.
- Visual appearance (alignment, effects) is deliberately unspecified — internal.

## 7. Scene/animation registry

Registry-based; names are stable semantic keys; bindings are internal data and may change without API change.

**Legacy → semantic mapping (frozen initial registry):**

| Legacy effect | Semantic key | Class |
|---|---|---|
| `demoCorners` | `corners` | animation |
| `demoChecker` | `checker` | animation |
| `demoBorder` | `border` | animation |
| `demoRowSweep` | `row_sweep` | animation |
| `demoBounce` | `bounce` | animation |
| `demoWave` | `wave` | animation |
| `demoRain` | `rain` | animation |
| `demoPlasma` | `plasma` | animation |
| preset `Rows` | `spectrum.rows` | spectrum style |
| preset `Columns` | `spectrum.columns` | spectrum style |
| preset `Mirror` | `spectrum.mirror` | spectrum style |
| preset `Rainbow` | `spectrum.rainbow` | spectrum style |
| preset `PeakDots` | `spectrum.peaks` | spectrum style |
| preset `CenterOut` | `spectrum.center_out` | spectrum style |
| preset `Blocks` | `spectrum.blocks` | spectrum style |
| preset `Fire` | `spectrum.fire` | spectrum style |
| Paint mode | diagnostic only (§5) | diagnostic |
| `showLinkStatus()` | derived indicator, not a scene | derived |
| Demo carousel | boot/demo convenience | non-semantic |

Distinction frozen: **scenes** = named persistent visual programs (incl. spectrum styles when spectrum is active); **animations** = motion effects usable as scenes or expression backdrops; **spectrum** = a mode whose style is a registry entry; **paint** = diagnostic. Adding registry entries never changes the API shape; unknown names return error `unknownScene`/`unknownAnimation` with the available list.

## 8. Expression contract

**Expression is V1 CORE** (locked refinement 1): LED Wall exists partly to be HUGO/LOOI's visual output, so expression intent is core product, not an add-on.

- Capability: `display.expression.set {expression}`; persistent baseMode command.
- **Frozen initial vocabulary:** `idle, listening, thinking, speaking, happy, alert, error, sleeping`.
- Semantics: clients request an **expression intent**; firmware maps it internally to a scene/animation/palette combination (e.g., `thinking`→slow `wave`, `alert`→`border` alert palette, `sleeping`→dim idle). **The mapping table is internal and may change without API version change**; clients must never depend on which renderer/effect realizes an expression; expressions must never be implemented as client-driven frame logic.
- Expressions contain **no HUGO-specific reasoning** — the vocabulary is generic device semantics usable by any client.
- Unknown expression ⇒ ERROR `unknownExpression` with the vocabulary list.
- The mapping table lives as versioned data (config/registry), not buried in code paths.

## 9. API/transport

- **Canonical transport:** HTTP for commands/queries (`POST` capability operations, `GET /state`, `GET /identity`), WebSocket for state/event push (`state.changed`, `text.done`, `transient.expired`, `mode.changed`). Bounded broadcast with slow-client drop (heritage hardening pattern).
- **Serial remains diagnostic only** — never the product contract.
- **MQTT / Home Assistant belongs in an external adapter**, not in the renderer/core (§17).
- **No automatic discovery in v1**; configured endpoint acceptable. Preserved verbatim: **Discovery does not imply authorization.** **No authentication in v1** — documented LAN-trust residual risk (consistent with Robot/Piano posture).
- **Error vocabulary (typed):** `textTooWide`, `unknownScene`, `unknownAnimation`, `unknownExpression`, `invalidParameter`, `unsupportedCapability`, `ledUnavailable` (only for LED-scoped internal failure worth surfacing), `internalError`. Raw FastLED/ADC exception strings never public.

## 10. Identity/capability handshake

- `deviceId`: stable `led-wall-<suffix>`; `deviceType`: `visual_display_v1`; `role`: `display`.
- `protocolVersion`: 1; `fwVersion`: semver.
- `capabilities[]`: the §5 core set (always) + conditional flags (`spectrum` only if mic hardware confirmed, `color`, `paint.diagnostic`).
- `matrix {width:30, height:10}` in identity — logical canvas is public.
- Health/state via `GET /state`; event subscription via WS.
- No automatic discovery; configured endpoint acceptable; **discovery does not imply authorization**; LAN-trust residual risk documented.

## 11. Arbitration / transient semantics

**Locked refinements 2 and 3, frozen exactly:**

### baseMode + one transient (no stack)
- LED Wall has exactly **one persistent `baseMode`**.
- Persistent commands (text/scene/animation/expression without `durationMs`) **replace `baseMode`** and **immediately cancel any active transient**.
- A command with `durationMs` creates **one temporary transient override**: at expiry, rendering returns to the current `baseMode`. A new transient **replaces** the previous transient (never queues, never nests). At most one transient exists at any time.
- **No nested transient stack. No notification queue in v1.**
- `text.clear` under this model: if text is the persistent baseMode ⇒ clear text, `baseMode = idle`; if text is the active transient ⇒ cancel that transient and return to the current baseMode. It never restores an arbitrary historical stack.

### CLEAR semantics (frozen)
`display.clear` means: cancel any active transient; clear the framebuffer; set `baseMode = idle`; set current mode = idle; produce **no** automatic animation; remain blank until another visual command is accepted. Always available, any client, any state.

### Client arbitration
- Multiple clients, **no ownership**; commands serialized centrally by the device.
- Accepted persistent command ⇒ new baseMode; accepted transient ⇒ replaces prior transient; `display.clear` always works.
- Malformed/unknown commands mutate nothing and return typed errors.
- Connection loss of any client changes nothing visually (session semantics, not a safety invariant).

## 12. Rendering reliability

### MUST NOT REGRESS
1. Matrix mapping correctness (serpentine + flip flags).
2. **Bounded rendering — realistic form:** FastLED/WS2812 physical frame transmission is inherently blocking internally (≈9 ms for 300 LEDs at 800 kHz plus overhead); the invariant is that this cost is **bounded** and scheduling (throttle + dirty-flag + main-loop discipline) **prevents it from starving network/command handling**. No zero-blocking promise is made.
3. Network callbacks never perform long LED work directly (heritage queue/drain discipline preserved).
4. `display.clear` always available and always effective.
5. Invalid commands never corrupt the renderer or state.
6. Safe known boot state (dim default/blank; never mid-animation garbage).
7. Brightness ceiling enforced locally regardless of client requests.
8. External connection loss does not corrupt LED state; last commanded visual or safe default persists.

### OPERATIONAL RELIABILITY
WiFi/WS reconnect with backoff (heritage 8 s pattern); config load with safe fallback; watchdog only if bring-up evidence justifies it; status/heartbeat on the event channel; serial diagnostics always available.

## 13. Brightness/power

Brightness heritage 40 is **evidence, not a safety certification**. Frozen: `display.brightness.set {0..100}` is semantic; the device maps it below a **locally enforced physical ceiling** (config; default conservative until measured). PSU/current/thermal limits remain production verification gates (§22) — no electrical claims are made by this document.

## 14. Microphone/spectrum

Local mic FFT spectrum is a **conditional capability**: existing DSP assets (capture/FFT/band pipeline, smoothing, 8 style renderers) are preserved regardless; the `spectrum` capability and `display.spectrum.start/stop` are advertised in `capabilities[]` **only if the microphone is physically confirmed**; if not, they are omitted and mic code may be compiled out — no architectural impact. Spectrum is a mode like any other (§10–§11), fully functional standalone, with **no Piano Core dependency** (the legacy in-firmware Pi consumption is not part of this capability).

## 15. Piano Core relationship

**LED Wall is independent.** Piano Core may later publish semantic events through an external adapter speaking LED Wall's public API. No shared state store, no shared secrets, no cross-project includes, no direct dependency in either direction. The hard-coded Pi :8766 dependency is **not preserved as product architecture** (it may exist temporarily as an internal/compat event adapter, never as the integration path). The Piano Core frozen specification remains unchanged.

## 16. HUGO/LOOI boundary

LED Wall is a future HUGO `DeviceAdapter` target. HUGO/LOOI may use: **text, scene, animation, expression, brightness, clear, state** (plus spectrum/color where capability-flagged). They must never handle LED indexes, GPIO, FastLED, renderer functions, or microphone internals. They request intent; the device renders. LED Wall contains no HUGO- or LOOI-specific reasoning; every client uses the same contract.

## 17. Home Assistant boundary

HA is an external adapter. Semantic mappings (evaluated, not forced): brightness/color as a light-like entity; **selects** for scenes/animations **and expression**; text service/action; clear/test buttons; state sensors. The whole device must **not** be forced into a single light entity where semantics do not fit. HA never sees GPIO, FastLED, serpentine math, or serial internals.

## 18. Runtime/config/persistence

One firmware process (single PlatformIO env). **Persist:** `deviceId`; config file (WiFi, brightness ceiling, boot default, expression mapping overrides, registry additions); logs (serial/journald-equivalent). **Never persisted:** current mode, transient timers, frame buffers, paint buffer, client sessions. Config changes apply with safe fallbacks; unknown config keys ignored with a warning.

## 19. Clean repository structure (proposal — do not create)

```text
led-wall/
  firmware/            # PlatformIO project (single env)
    src/core/          # lifecycle, mode machine, baseMode/transient logic, config
    src/render/        # frame buffer, throttle, matrix mapping (seed: eq_screen.h, eq_render.cpp)
    src/effects/       # animation/scene registry + renderers (seed: demos, eq_presets.cpp)
    src/text/          # font + text renderer (seed: font5x7.h, drawString)
    src/audio/         # mic FFT pipeline (seed: eq_audio.cpp) — conditional capability
    src/api/           # HTTP + WS contract (new; legacy Pi-link = optional compat adapter)
    include/           # config + own secrets example (no cross-repo includes)
  hardware/            # wiring, PSU notes, orientation verification, panel photos
  docs/                # CONTRACT.md, DECISIONS.md, runbook
  tests/               # mapping, text-width, error-vocabulary, transient-timer tests
```

Minimal by intent; no `ui/` — control comes from dashboards/HUGO/HA.

## 20. Legacy extraction map

`CURRENT PATH (PLUTO EQ ACCESSORY) → ASSET → FUTURE ROLE → ACTION`

| Current path | Asset | Future role | Action |
|---|---|---|---|
| `include/eq_screen.h` | serpentine XY mapping + flip flags | `src/render/` | COPY/ADAPT |
| `include/font5x7.h` | 5×7 bitmap font | `src/text/` | COPY/ADAPT |
| `src/main.cpp` text funcs (`glyphCols/drawChar/drawString/stringWidth`) | text rendering | `src/text/` | COPY/ADAPT |
| `src/eq_presets.cpp` | 8 spectrum style renderers | `src/effects/` registry | COPY/ADAPT |
| `src/main.cpp` demos (`demoCorners…demoPlasma`) | animation renderers | `src/effects/` registry | COPY/ADAPT |
| `include/eq_render.h` + `src/eq_render.cpp` | dirty-flag 20 FPS throttle | `src/render/` | COPY/ADAPT |
| `src/eq_audio.cpp` + FFT config | mic capture/FFT/bands | `src/audio/` (conditional) | COPY/ADAPT |
| `src/eq_piano_viz.cpp` | note→column visualizer | effects registry entry (source-agnostic) | COPY/ADAPT |
| `Paint` (`paintSet/paintDraw`) | diagnostic pixel painting | diagnostic capability | COPY/ADAPT |
| `src/main.cpp` (mode machine, serial CLI, `setup/loop`) | app skeleton | `src/core/` + `src/api/` | REWRITE CLEANLY |
| `include/eq_app.h` + `src/eq_app.cpp` | mode/preset globals | `src/core/` state model (baseMode/transient) | REWRITE CLEANLY |
| `include/eq_config.h` | verified constants | `include/` config + `hardware/NOTES.md` | PRESERVE DATA |
| `src/eq_pi_link.cpp` + `include/eq_pi_link.h` | Pi :8766 WS client | optional compat event adapter only | COMPATIBILITY LAYER (optional) |
| `include/eq_wifi_config.h` + `eq_secrets*.h` | shared WiFi/secrets includes | own isolated config/secrets | DISCARD (replace) |
| `src/eq_portal.cpp` + `include/eq_portal.h` | AP portal (compiled out) | — | ARCHIVE |
| `platformio.ini` | build config | root build config | COPY/ADAPT |

## 21. Technical debt to discard

Hard-coded Pi IP/`EQ_WS_HOST` piano coupling; shared secrets include chain into `PLUTO PIANO BRAIN/include`; passive Pi dependency as the only smart input; serial as the only command surface; dead AP portal; 800-line flat demo/product `main.cpp`; magic palette/timing numbers; PLUTO/EQ naming coupling (device is a display, not an "EQ accessory"); Albanian-only inline comments as sole documentation; undocumented brownout-disable power workaround; zero persistence of settings.

## 22. Physical verification gates

### USER/REPOSITORY VERIFIED (no re-proving for architecture)
ESP32 devkit family; WS2812; 300 LEDs; 30×10; 10 rows; GPIO13; serpentine + odd-row reversal; GPIO34 mic heritage; brightness-40 heritage; ≈20 FPS throttle; firmware compiled 2026-05-26.

### TEST-READY PHYSICAL CHECKS
Exact board model; strip orientation vs flip flags on the physical panel; wall-mount orientation; microphone hardware present/wired; identity of currently flashed firmware; boot behavior on real power.

### PRODUCTION VERIFICATION
PSU voltage/current and draw at the chosen brightness ceiling; shared ground; voltage drop/color consistency across 10 strips; thermal behavior wall-mounted; flicker/noise with WiFi; brightness ceiling value agreement; dimensions/mounting; full §5 core-capability smoke pass.

## 23. Frozen non-goals

Part of Piano Core; raw LED-index/FastLED/GPIO public API; image/photo/video rendering; multi-zone layouts; notification queues/overlay stacks; nested transients; automatic discovery; authentication (v1); cloud dependency; MQTT inside the firmware (adapter-external); HUGO/LOOI-specific code; mic as a mandatory capability; per-client ownership/arbitration machinery; database persistence; renaming the verified matrix facts.

## 24. Implementation PASS/FAIL checklist

**Core runtime**
- [ ] Single-firmware mode machine with exactly one `baseMode` and at most one transient — PASS/FAIL
- [ ] State model == §4; public/internal separation enforced — PASS/FAIL
- [ ] Safe known boot state; visual state not persisted — PASS/FAIL
- [ ] Config persisted; ceiling/boot-default/mappings load with fallbacks — PASS/FAIL

**Render/effects**
- [ ] Matrix mapping byte-faithful to heritage (serpentine + flips verified on panel) — PASS/FAIL *(INHERITED-PRESERVE)*
- [ ] Bounded render throttle preserved (~20 FPS, dirty-flag) — PASS/FAIL *(INHERITED-PRESERVE)*
- [ ] Network callbacks never perform long render work directly — PASS/FAIL *(INHERITED-PRESERVE)*
- [ ] All 13 heritage animations + 8 spectrum styles registered under semantic names — PASS/FAIL *(COPY/ADAPT)*

**Text**
- [ ] 5×7 font + scrolling preserved; `textTooWide`, `skippedCharacters`, 128-char cap, speed bounds implemented — PASS/FAIL *(NEW contract over INHERITED renderer)*

**API**
- [ ] All §5 core capabilities via HTTP + WS per §9 — PASS/FAIL *(NEW)*
- [ ] Typed error vocabulary == §9 — PASS/FAIL *(NEW)*
- [ ] `durationMs` transient semantics per §11 (replace-not-queue, expiry→baseMode) — PASS/FAIL *(NEW)*
- [ ] `display.clear` per §11 (cancel transient, blank, idle, no auto-animation) — PASS/FAIL
- [ ] Legacy Pi consumption confined to optional compat adapter — PASS/FAIL *(COMPAT, optional)*

**Expression**
- [ ] `display.expression.set` V1 core; vocabulary == 8 frozen intents; mappings data-driven; unknown ⇒ `unknownExpression` — PASS/FAIL *(NEW)*

**LED/brightness**
- [ ] Brightness with locally enforced ceiling; requests clamped — PASS/FAIL
- [ ] LED failure/`ledUnavailable` never reported as crash; device keeps serving — PASS/FAIL

**Mic (conditional)**
- [ ] `spectrum` capability advertised **only if mic confirmed**; start/stop as mode — PASS/FAIL *(CONDITIONAL)*

**Boundaries**
- [ ] HUGO/LOOI boundary == §16; zero HUGO/LOOI code in firmware — PASS/FAIL
- [ ] HA via external adapter only; no forced single light entity — PASS/FAIL
- [ ] Piano Core independence per §15; no Pi :8766 product dependency — PASS/FAIL *(DISCARD-LEGACY: hard-coded Pi link as architecture)*

**Docs/tests**
- [ ] CONTRACT.md (capabilities/errors/transient semantics), DECISIONS.md, runbook — PASS/FAIL
- [ ] Mapping, text-width, error-vocabulary, transient-timer tests — PASS/FAIL

**Hardware verification**
- [ ] §22 TEST-READY items each executed — PASS/FAIL
- [ ] §22 PRODUCTION items executed or explicitly deferred with reasons — PASS/FAIL

## 25. Consistency notes

Targeted checks before freezing:
1. **Expression V1 core everywhere** — §1 (product), §5 (core table), §8 (contract), §10 (capabilities), §16 (HUGO may use), §17 (HA expression select), §24 (dedicated checklist group). No remaining "OPTIONAL V1" label on expressions. ✔
2. **baseMode/transient contains no hidden stack** — §4 (exactly one transient field), §11 (replace-not-queue; persistent command cancels transient; expiry returns to baseMode), §24 timer test; "no nested stack, no notification queue" stated as frozen non-goals (§23). ✔
3. **Clear semantics unambiguous** — §11 defines CLEAR-1 (`display.clear`) and CLEAR-2 (`display.text.clear`) distinctly, including the transient-vs-base branch; arbitration (§10) and checklist (§24) reference them; no historical-stack restoration anywhere. ✔
4. **Scene vs animation coherence** — §7 registry separates classes, names, and the diagnostic paint exception; spectrum styles are registry entries of a mode; legacy function names absent from the API. ✔
5. **LED hardware internals hidden** — §3 boundary table, §5 (no raw coordinates except flagged diagnostic paint), §16–§17; `matrix` size exposed as logical canvas only. ✔
6. **Mic capability conditional** — §14 + §10 + §24; DSP assets preserved regardless. ✔
7. **HUGO/HA/Piano external** — §15/§16/§17; legacy Pi link demoted to optional compat (§15, §20). ✔
8. **Render timing physically realistic** — §12 states bounded blocking (≈9 ms frame transmission) with scheduling protection; no zero-blocking promise. ✔
9. **Verified hardware untouched** — §2 preserves every baseline fact verbatim. ✔

No unresolved alternatives remain on any checked point.

`SPECIFICATION CONSISTENCY CHECK: PASS`

NO IMPLEMENTATION AUTHORIZED BY THIS DOCUMENT
