# ADR — LED-L006: HUGO DeviceAdapter as an external intent-only tool surface

- **Status:** PROPOSED (repository decision for the authorized LED-L006 milestone; executor-recorded, pending independent review)
- **Task:** LED-L006
- **Date:** 2026-09-13
- **Authority:** Owner authorization "LED-L006 in full" (Redion Cekrezi, 2026-09-13). Frozen spec §16/§1/§3/§5/§23, ADR-001/002/005/014, PLAN Phase 5, and the implementation gate line "HUGO/LOOI boundary == §16; zero HUGO/LOOI code in firmware" remain the normative constraints; this record documents how LED-L006 implements them.

## Context

Frozen spec §16 and ADR-014 freeze the HUGO/LOOI boundary: LED Wall is a future HUGO
`DeviceAdapter` target; HUGO may use **text, scene, animation, expression, brightness,
clear, state** (plus spectrum/color where capability-flagged), must never handle LED
indexes, GPIO, FastLED, renderer functions, or microphone internals, and requests
*intent* while the device renders. §1 excludes HUGO/LOOI reasoning from LED Wall
ownership; §23 and the implementation gate require **zero HUGO/LOOI-specific code in
firmware**; ADR-005 freezes HTTP+WS as the canonical control model specifically so
HUGO-class adapters are uniform. PLAN Phase 5 adds: LED Wall must be fully functional
without HUGO. The repository's LED-L005 milestone established the external-adapter
precedent (host-side Python 3 stdlib-only package, pure API client, zero firmware
changes, deterministic fake-infrastructure tests).

No HUGO runtime, SDK, or specification exists inside this repository, and no live
HUGO/device authorization exists for V3+ checks — LED-L006 must therefore deliver the
LED-Wall-side half of the DeviceAdapter boundary: a deterministic, host-consumable
intent surface over the accepted LED-L003 API.

## Decision

1. **A host-side external DeviceAdapter under `hugo_adapter/`** (Python 3, standard
   library only) is the LED-L006 deliverable. It speaks the accepted LED-L003 HTTP API
   to the device (device client semantics COPY-ADAPT from the accepted LED-L005
   `ha_adapter/device.py`, kept independent so the two adapters never couple) and
   exposes an **intent-level tool surface** for the HUGO host. The firmware receives
   **zero changes** — the accepted LED-L004/LED-L005 firmware artifact must be
   reproduced byte-identically by the LED-L006 verification build.
2. **The HUGO-facing transport is JSON-RPC 2.0 over stdio** (line-delimited UTF-8
   JSON; responses on stdout, logs on stderr). Rationale: the HUGO side of the
   `DeviceAdapter` concept is not specified by any repository document, so the
   adapter defines the deterministic LED-Wall-side contract any HUGO host (or
   operator) can consume: a subprocess speaking a small, publicly normative,
   curl/debug-testable protocol. No listening socket is opened (no new network
   surface); the tool/intent layer is transport-independent if a host needs a bridge.
   Methods: `ping`, `tools/list`, `tools/call`. Batches are rejected with a single
   `-32600` error (documented limitation); notifications are executed but never
   answered (JSON-RPC 2.0 §4). Line length is capped (64 KiB).
3. **The tool catalog is exactly the §16 allowed set**, mapped 1:1 onto CONTRACT.md
   endpoints: `show_text`, `clear_text`, `set_scene`, `set_animation`,
   `set_expression`, `set_brightness`, `clear_display`, `get_state`,
   `get_device_info`. Conditional §16 tools (`set_color`, `start_spectrum`,
   `stop_spectrum`) exist **only when the capability is advertised** in
   `capabilities[]` (currently omitted → hidden; fail-closed local
   `unsupportedCapability` rejection otherwise, no device call). No tool exposes raw
   coordinates, renderer controls, or LED indexes — the catalog is fixed data, so the
   boundary is mechanical, not conventional.
4. **Deterministic device policy lives in the adapter; the AI client stays above
   it.** Arguments are validated locally and fail-closed (bounds per §5/§6/
   CONTRACT.md: text 1..128, speedMsPerPx 40..500, durationMs 1..2³¹−1, level
   0..100, bools-as-ints rejected, unknown/extra keys rejected) — malformed input
   mutates nothing and never reaches the device. Registry membership stays
   device-authoritative (unknown names are forwarded and the §9 typed error with its
   available list is surfaced to the AI). Frozen §7/§8 vocabularies are embedded in
   tool descriptions only (the API does not enumerate registries; documented
   limitation, LED-L005 precedent).
5. **Capability discovery is TTL-bounded and never implies authorization** (§10):
   `get_device_info` returns identity+capabilities; conditional-tool eligibility
   uses a `GET /identity` cache (default 5 s, clamped 1..300) whose refresh failure
   fails **closed** (conditional tools hidden until a refresh succeeds; core tools
   unaffected — they are always advertised). Control remains the same fixed tool set
   for every client in the documented v1 LAN-trust posture.
6. **No adapter-side visual state.** The device is authoritative; every mutating tool
   is a single validated HTTP command (single attempt — no retry storms, no duplicate
   non-idempotent delivery); `get_state` is always live. The service is a sequential
   single-threaded stdio loop: no background growth, no session state, no state
   resurrection on disconnect/restart.
7. **Config with safe fallbacks (§18 pattern), no credentials in v1.** The runtime
   config (`hugo_adapter/config.json`, git-ignored) carries the device URL, timeouts,
   and capability TTL; the example file carries placeholders. v1 API authentication
   does not exist, so the adapter has no credential surface; the LAN-trust residual
   is inherited and documented.

## Consequences

- `firmware/` and `ha_adapter/` are untouched; LED-L001..L005 wire behavior is
  byte-preserved (enforced by C-L006-4/C-L006-6).
- The adapter requires Python ≥3.8 on the host running it; HUGO-side deployment is
  user infrastructure (no deployment performed in this task).
- Live HUGO integration is host-verified only via deterministic fake-device
  integration tests (V2); V3/V4/V5/V6 stay pending HUGO-runtime/device/infrastructure
  authorization.
- A HUGO host wanting a different transport (HTTP, MCP-style framing) must bridge it
  itself; this repository deliberately does not speculate a HUGO SDK (§23 non-goal,
  no speculative post-roadmap architecture).
