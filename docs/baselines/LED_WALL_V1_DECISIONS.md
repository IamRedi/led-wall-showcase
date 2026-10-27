# LED WALL V1 DECISIONS RECORD

Concise ADRs for the frozen LED Wall v1 architecture. Status of every entry: **ACCEPTED**.
Alternatives are not reopened; changes require a new revision of `LED_WALL_V1_FROZEN_SPEC.md`.

---

## ADR-001 — LED Wall is one standalone visual device
- **Decision:** LED Wall is its own logical device, independent of Piano Core and every other system.
- **Evidence/rationale:** self-contained 300-LED matrix with its own mic, firmware, and identity; Piano Core frozen spec already excludes EQ as separate (ADR-012 there); locked product direction 1.
- **Consequence:** its own repository, identity, config, and API; other systems integrate only through its public API.
- **Status: ACCEPTED**

## ADR-002 — ESP32 remains the local renderer/controller
- **Decision:** the ESP32 owns physical rendering, matrix mapping, frame timing, animations, text, brightness, local visual state, optional mic mode, and any local current limits.
- **Evidence/rationale:** deployed, user-verified firmware does exactly this on proven hardware.
- **Consequence:** external clients never manipulate GPIO/FastLED details; the device works fully standalone.
- **Status: ACCEPTED**

## ADR-003 — 30×10 / 300-LED matrix facts are preserved
- **Decision:** the verified hardware facts (WS2812, 300 LEDs, 30×10, 10 strips, GPIO13, serpentine + odd-row reversal, GPIO34 mic heritage, brightness-40 heritage, ~20 FPS throttle) are frozen as evidence/configuration.
- **Evidence/rationale:** repository-verified constants (`eq_config.h`, `eq_screen.h`, `eq_render.cpp`); locked direction 3.
- **Consequence:** these are evidence/configuration, not all public API; only logical matrix size is public.
- **Status: ACCEPTED**

## ADR-004 — Public API is semantic; FastLED/GPIO/LED indexes are internal
- **Decision:** only semantic capabilities are public; renderer internals are replaceable.
- **Evidence/rationale:** HUGO/LOOI/HA need intent, not hardware; boundary table in spec §3.
- **Consequence:** single exception is the diagnostic `display.paint.*` capability, explicitly flagged.
- **Status: ACCEPTED**

## ADR-005 — HTTP commands/queries + WebSocket events is the canonical v1 control model
- **Decision:** freeze HTTP+WS as the control architecture; serial stays diagnostic; MQTT belongs to an external HA adapter.
- **Evidence/rationale:** matches the Piano Core canonical pattern for uniform HUGO adapters; curl-debuggable; HA-friendly; the baseline comparison favored B over WS-only or MQTT-first.
- **Consequence:** one contract shape for all clients; MQTT never enters the firmware.
- **Status: ACCEPTED**

## ADR-006 — `display.expression.set` is V1 CORE
- **Decision:** expression intent is core product, promoted from OPTIONAL per locked refinement 1.
- **Evidence/rationale:** LED Wall's primary future role is HUGO/LOOI visual feedback; expressions map to existing effects cheaply.
- **Consequence:** expression appears in identity capabilities, state model, HUGO boundary, HA mapping, and checklist as core.
- **Status: ACCEPTED**

## ADR-007 — Expression vocabulary starts with the 8 frozen intents
- **Decision:** initial vocabulary: `idle, listening, thinking, speaking, happy, alert, error, sleeping`.
- **Evidence/rationale:** covers the HUGO/LOOI interaction states with effects already available (wave/border/rain/idle-dim); generic device semantics, no HUGO-specific reasoning.
- **Consequence:** mappings are internal data, changeable without API change; unknown ⇒ `unknownExpression`.
- **Status: ACCEPTED**

## ADR-008 — State model uses one persistent baseMode + at most one transient
- **Decision:** persistent commands replace baseMode (cancelling any transient); `durationMs` commands create a single transient that expires back to baseMode; new transient replaces prior; no stack.
- **Evidence/rationale:** locked refinement 2; deterministic and sufficient for text/scene/animation/expression needs; avoids notification-queue complexity.
- **Consequence:** no overlay stack in v1; state exposes `baseMode`, `currentMode`, `activeTransient`.
- **Status: ACCEPTED**

## ADR-009 — `display.clear` always cancels transient, blanks framebuffer, returns to idle
- **Decision:** CLEAR-1 semantics frozen: cancel transient + clear + `baseMode = idle` + no automatic animation; `display.text.clear` (CLEAR-2) is scoped to text base/transient only.
- **Evidence/rationale:** locked refinement 3; the display analog of the always-works stop verb on other household devices.
- **Consequence:** no historical-stack restoration; clear is callable by any client in any state.
- **Status: ACCEPTED**

## ADR-010 — No notification queue or nested overlays in v1
- **Decision:** deferred entirely; future notification support will be a documented priority-class extension.
- **Evidence/rationale:** locked refinement 2; queueing adds ownership/priority complexity a display does not need yet.
- **Consequence:** transient replacement is the only overlap behavior in v1.
- **Status: ACCEPTED**

## ADR-011 — Brightness requests are constrained by a device-local physical ceiling
- **Decision:** semantic brightness 0–100 is clamped by a config-enforced local ceiling; heritage 40 is evidence, not a safety guarantee.
- **Evidence/rationale:** no PSU/current evidence exists for 300 LEDs; ceiling defaults conservative until production verification.
- **Consequence:** no electrical claims; ceiling value is a §22 production gate.
- **Status: ACCEPTED**

## ADR-012 — Microphone/spectrum is a conditional capability
- **Decision:** spectrum advertised in `capabilities[]` only if mic hardware is physically confirmed; DSP assets preserved regardless.
- **Evidence/rationale:** mic wiring on the physical panel is unverified; capability-gating avoids advertising hardware that may be absent.
- **Consequence:** zero architectural impact either way; spectrum is a normal mode, Piano-Core-independent.
- **Status: ACCEPTED**

## ADR-013 — Piano Core is not a dependency; integration is external through the semantic API
- **Decision:** LED Wall is independent; Piano Core may publish events via an external adapter; the hard-coded Pi :8766 link is not product architecture.
- **Evidence/rationale:** Piano Core frozen spec excludes EQ as separate; coupling would recreate the multi-provider tangle the architecture eliminated.
- **Consequence:** no shared state/secrets/includes; legacy `eq_pi_link.cpp` survives only as an optional compat adapter.
- **Status: ACCEPTED**

## ADR-014 — HUGO/LOOI use LED Wall through a DeviceAdapter and request visual intent only
- **Decision:** HUGO/LOOI may request text, scene, animation, expression, brightness, clear, state; never LED indexes, GPIO, FastLED, renderer functions, or mic internals.
- **Evidence/rationale:** locked direction 5/17; intent-level control keeps the device renderer replaceable.
- **Consequence:** zero HUGO/LOOI-specific code in firmware; one contract serves all clients.
- **Status: ACCEPTED**

## ADR-015 — Home Assistant is an external adapter
- **Decision:** HA integrates via an external adapter (expected MQTT, per Piano Core precedent), mapping brightness/color, scene/animation/expression selects, text action, clear button, state sensors.
- **Evidence/rationale:** keeps MQTT out of the firmware; matches the household adapter pattern; semantics must not be forced into a single light entity.
- **Consequence:** HA never sees GPIO/FastLED/serpentine/serial internals.
- **Status: ACCEPTED**

## ADR-016 — Legacy PLUTO EQ naming/Pi coupling does not survive as product architecture
- **Decision:** "EQ accessory" naming and the hard-coded Pi dependency are discarded; the device is LED Wall, a display.
- **Evidence/rationale:** discovery audit showed naming and coupling are historical accidents, not product truths; technical-debt list confirmed at freeze.
- **Consequence:** clean identity (`led-wall-*`), own secrets/config, serial demoted to diagnostics, portal archived.
- **Status: ACCEPTED**
