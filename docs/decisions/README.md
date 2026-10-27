# Architecture Decision Records (Repository-Level)

> Hinweis zur öffentlichen Fassung: Verweise auf `docs/work/` (Aufgabenliste, Status, Nachweise) und auf den internen Arbeitsstandard (RWS) beziehen sich auf private Entwicklungsunterlagen, die hier nicht veröffentlicht sind. Den Stand fasst [docs/PROJEKTSTAND.md](../PROJEKTSTAND.md) zusammen.

This directory holds **repository-level ADRs** for `led-wall` — decisions about how this
repository is governed and run (process, tooling, verification semantics).

**Product/architecture decisions for LED Wall V1 are NOT recorded here.** They are frozen in
[`docs/baselines/LED_WALL_V1_DECISIONS.md`](../baselines/LED_WALL_V1_DECISIONS.md)
(ADR-001 through ADR-016, all ACCEPTED, part of the frozen baseline). Frozen baseline files are
never modified; changes go through a new amendment artifact.

## Conventions

- Filename: `ADR-<SHORT-NAME>.md` (repository-level decisions).
- Each ADR states: context, decision, alternatives considered, consequences, and status.
- Statuses follow RWS conventions (`PROPOSED`, `ACCEPTED`, `SUPERSEDED`, …).
- ADRs are canonical once ACCEPTED; superseding an ADR requires naming the successor.

## Index

| ADR | Title | Status |
|---|---|---|
| [ADR-LED-L007-RENDERER-PIPELINE.md](ADR-LED-L007-RENDERER-PIPELINE.md) | Renderer pipeline: heritage renderers behind the semantic state, deterministic palette, data-driven expressions | ACCEPTED (LED-L007, pending milestone review) |
