# LED WALL V1 IMPLEMENTATION GATE

Implementation status: NOT STARTED

Implementation may begin only against `LED_WALL_V1_FROZEN_SPEC.md`
(LED WALL V1 SPECIFICATION — FROZEN v1.0, consistency check: PASS).

Status:

- Planning: COMPLETE
- Architecture: FROZEN v1.0
- Implementation: NOT STARTED
- Hardware reconnection: NOT REQUIRED FOR ARCHIVE
- Production validation: PENDING

Rules of use:

- Every line is PASS/FAIL at implementation time; nothing may be skipped, merged, or reinterpreted.
- Labels: *(INHERITED-PRESERVE)* = proven deployed behavior to preserve verbatim;
  *(COPY/ADAPT)* = heritage code adapted; *(NEW)* = to implement;
  *(CONDITIONAL)* = capability advertised only if hardware confirmed;
  *(COMPAT)* = compatibility obligations; *(DISCARD-LEGACY)* = legacy intentionally not carried.
- No line may be marked PASS without actual implementation evidence (code + test/verification output).

## Core runtime

- [ ] Single-firmware mode machine with exactly one `baseMode` and at most one transient — PASS/FAIL
- [ ] State model == spec §4; public/internal separation enforced — PASS/FAIL
- [ ] Safe known boot state; visual state not persisted — PASS/FAIL
- [ ] Config persisted; ceiling/boot-default/mappings load with fallbacks — PASS/FAIL

## Render/effects

- [ ] Matrix mapping byte-faithful to heritage (serpentine + flips verified on panel) — PASS/FAIL *(INHERITED-PRESERVE)*
- [ ] Bounded render throttle preserved (~20 FPS, dirty-flag) — PASS/FAIL *(INHERITED-PRESERVE)*
- [ ] Network callbacks never perform long render work directly — PASS/FAIL *(INHERITED-PRESERVE)*
- [ ] All 13 heritage animations + 8 spectrum styles registered under semantic names — PASS/FAIL *(COPY/ADAPT)*

## Text

- [ ] 5×7 font + scrolling preserved; `textTooWide`, `skippedCharacters`, 128-char cap, speed bounds implemented — PASS/FAIL *(NEW contract over INHERITED renderer)*

## API

- [ ] All §5 core capabilities via HTTP + WS per §9 — PASS/FAIL *(NEW)*
- [ ] Typed error vocabulary == §9 — PASS/FAIL *(NEW)*
- [ ] `durationMs` transient semantics per §11 (replace-not-queue, expiry→baseMode) — PASS/FAIL *(NEW)*
- [ ] `display.clear` per §11 (cancel transient, blank, idle, no auto-animation) — PASS/FAIL
- [ ] Legacy Pi consumption confined to optional compat adapter — PASS/FAIL *(COMPAT, optional)*

## Expression

- [ ] `display.expression.set` V1 core; vocabulary == 8 frozen intents (idle, listening, thinking, speaking, happy, alert, error, sleeping); mappings data-driven; unknown ⇒ `unknownExpression` — PASS/FAIL *(NEW)*

## LED/brightness

- [ ] Brightness with locally enforced ceiling; requests clamped — PASS/FAIL
- [ ] LED failure/`ledUnavailable` never reported as crash; device keeps serving — PASS/FAIL

## Mic (conditional)

- [ ] `spectrum` capability advertised **only if mic confirmed**; start/stop as mode — PASS/FAIL *(CONDITIONAL)*

## Boundaries

- [ ] HUGO/LOOI boundary == §16; zero HUGO/LOOI code in firmware — PASS/FAIL
- [ ] HA via external adapter only; no forced single light entity — PASS/FAIL
- [ ] Piano Core independence per §15; no Pi :8766 product dependency — PASS/FAIL *(DISCARD-LEGACY: hard-coded Pi link as architecture)*

## Docs/tests

- [ ] CONTRACT.md (capabilities/errors/transient semantics), DECISIONS.md, runbook — PASS/FAIL
- [ ] Mapping, text-width, error-vocabulary, transient-timer tests — PASS/FAIL

## Hardware verification

- [ ] §22 TEST-READY items each executed (board model, orientation, wall-mount, mic, flashed firmware) — PASS/FAIL
- [ ] §22 PRODUCTION items executed or explicitly deferred with reasons (PSU/current, ground, thermals, ceiling value, smoke pass) — PASS/FAIL
