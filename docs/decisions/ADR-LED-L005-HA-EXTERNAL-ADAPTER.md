# ADR — LED-L005: Home Assistant integration via an external MQTT adapter

- **Status:** ACCEPTED (repository decision for the authorized LED-L005 milestone; executor-recorded, pending independent review)
- **Task:** LED-L005
- **Date:** 2026-09-13
- **Authority:** Owner authorization "LED-L005 in full" (Redion Cekrezi, 2026-09-13). Frozen spec §17/§9/§23, ADR-015, PLAN Phase 4 remain the normative constraints; this record documents how LED-L005 implements them.

## Context

Frozen spec §17 and ADR-015 freeze the Home Assistant boundary: HA is an **external adapter** client of the semantic API, **MQTT stays out of the firmware** (ADR-004 freezes HTTP+WS as the only firmware transport), mappings are "evaluated, not forced", and the device must not be forced into a single light entity. The repository had no adapter code, no MQTT heritage in the legacy firmware, and no live device/broker authorization for V3+ checks.

## Decision

1. **A host-side external adapter under `ha_adapter/`** (Python 3, **standard library only**) is the LED-L005 deliverable. It speaks the accepted LED-L003 HTTP API to the device and MQTT 3.1.1 to the household broker. The firmware receives **zero changes** — the accepted LED-L004 firmware artifact must be reproduced byte-identically by the LED-L005 verification build.
2. **MQTT client is a bounded 3.1.1 subset** implemented in-repo (CONNECT/CONNACK, PUBLISH QoS 0/1, SUBSCRIBE QoS 1, PUBACK, PINGREQ/PINGRESP, DISCONNECT, last-will, capped remaining-length guard, capped exponential reconnect backoff, `clean_session=false` with a stable client id). Rationale: no third-party MQTT library is installed on the verification host and no network installs are authorized; a stdlib-only adapter keeps the permanent test suite deterministic and self-contained. The client is an internal, replaceable module (spec §3 right-column rule). No QoS 2, no TLS in v1 — consistent with the documented LAN-trust posture.
3. **Device state via HTTP polling** (`GET /state`, configurable interval, default 5 s, clamped ≥1 s), with an immediate rate-limited re-poll after each accepted command. The WebSocket event channel is intentionally **not** used by the adapter in v1: a stdlib RFC 6455 client would add protocol surface the canonical scope does not require, and polling is the accepted freshness pattern in this repository (LED-L004 dashboard fallback; N-L004-1 accepted polling degradation). Consequence: short transients can be missed between polls — documented limitation, device remains authoritative.
4. **Entity mapping (ADR-015 table realized):**
   - `light` — brightness 0..100 → `display.brightness.set`; on/off = drive level (off → level 0, on → restore last nonzero level, heritage default 40 if none seen). The device has no true power-off; this mapping is content-preserving and never resurrects stale device state.
   - `select` ×3 — scenes and animations (frozen §7 registry: the 8 animation keys usable as scenes) and expression (frozen §8 vocabulary, 8 intents incl. `alert` — this is the "alert/expression actions" surface).
   - `text` — 128-char capped text action → `display.text.show` (persistent form).
   - `button` — clear → `display.clear` (§11 CLEAR-1). The §17 "clear/test buttons" pair is realized as clear only: the v1 API has no test capability, and inventing one is out of scope.
   - `sensor` — currentMode (+ bounded attributes: baseMode, active program names, transient, brightness, text, uptime); `binary_sensor` — device reachability.
   - Color/spectrum entities are created **only if advertised** in `capabilities[]` (currently omitted, §5/§14/ADR-012).
5. **Home Assistant MQTT discovery is used (configurable, default on, retained).** Boundary interpretation, recorded for reviewer adjudication: the §9/§23 "no automatic discovery" non-goal binds the **device/API** (the device never advertises itself; the adapter must be explicitly configured with device URL and broker). Publishing MQTT discovery configs is the consumer-side entity-registration mechanism inside the already-configured adapter↔broker relationship — the device performs no discovery behavior. Discovery can be disabled in the adapter config.
6. **Secret hygiene:** real credentials live only in the uncommitted runtime config (`ha_adapter/config.json`, git-ignored); `config.example.json` carries placeholders.

## Consequences

- `firmware/` is untouched; LED-L001..L004 wire behavior is byte-preserved (enforced by verification C-L005-4/C-L005-6).
- The adapter requires Python ≥3.8 on the host running it; the household broker and adapter deployment are user infrastructure (no deployment performed in this task).
- Live Home Assistant behavior is host-verified only via deterministic fake-broker/fake-device integration tests (V2); V3/V4/V5/V6 stay pending device/infrastructure authorization.
- Frozen vocabularies are embedded in the adapter because the API does not enumerate registries; device-side registry additions require an adapter update (documented limitation).
