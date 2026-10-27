# LED Wall HUGO DeviceAdapter (LED-L006)

External host-side adapter that exposes the LED Wall as an **intent-only tool
surface** for HUGO-class intelligent clients, per frozen spec §16 / ADR-014:
HUGO may use **text, scene, animation, expression, brightness, clear, state**
(plus spectrum/color only where the device advertises the capability); it never
sees LED indexes, GPIO, FastLED, renderer functions, or microphone internals.
The AI client requests intent; the device renders. LED Wall stays fully
functional without this adapter; **zero firmware changes** (ADR-LED-L006).

- Python 3, **standard library only**; no network installs.
- A pure client of the accepted LED-L003 semantic API (`docs/api/CONTRACT.md`).
- Deterministic device policy lives in the adapter; the AI stays above it.

## Running

```bash
python3 -m hugo_adapter --config hugo_adapter/config.json
# or:  python3 -m hugo_adapter --device http://192.168.1.50
```

Copy `config.example.json` to `config.json` (git-ignored) and set `device_url`.
v1 has no credentials (documented LAN-trust posture), so the runtime config
holds no secrets. Exit codes: 0 on stdin EOF or SIGTERM, 2 on config error.

## The stdio JSON-RPC surface

Line-delimited UTF-8 JSON: one request per line on stdin, one response per
line on stdout. **stdout is pure protocol — all logs go to stderr.**
JSON-RPC 2.0 (batches are rejected with a single `-32600` error;
notifications are executed but never answered, per the spec).

### Methods

| Method | Params | Result |
|---|---|---|
| `ping` | `{}` | `{"pong": true, "service": "ledwall-hugo-deviceadapter"}` |
| `tools/list` | `{}` | `{"tools": [{name, description, requiresCapability?}, …]}` |
| `tools/call` | `{"name": str, "arguments"?: object}` | `{"ok": true, "tool": …}` plus `state` / `identity` for queries |

### Tools (frozen §16 surface, mapped 1:1 to CONTRACT.md endpoints)

| Tool | Device endpoint | Notes |
|---|---|---|
| `show_text` | `POST /display/text/show` | text 1..128 chars, `scroll` bool, `speedMsPerPx` 40..500 (default 140), optional `durationMs` (transient) |
| `clear_text` | `POST /display/text/clear` | §11 CLEAR-2 |
| `set_scene` | `POST /display/scene/set` | registry names; optional `durationMs` |
| `set_animation` | `POST /display/animation/set` | registry names; optional `durationMs` |
| `set_expression` | `POST /display/expression/set` | frozen vocabulary `idle, listening, thinking, speaking, happy, alert, error, sleeping`; persistent (no `durationMs`) |
| `set_brightness` | `POST /display/brightness/set` | level 0..100 (device clamps to its ceiling) |
| `clear_display` | `POST /display/clear` | §11 CLEAR-1; always available |
| `get_state` | `GET /state` | live §4 public state summary |
| `get_device_info` | `GET /identity` | identity + capabilities + logical matrix size; discovery only — grants no control |
| `set_color` *(conditional)* | `POST /display/color/set` | only when `display.color.set` is advertised (currently omitted → hidden) |
| `start_spectrum` *(conditional)* | `POST /display/spectrum/start` | only when `display.spectrum.start` is advertised (currently omitted → hidden) |
| `stop_spectrum` *(conditional)* | `POST /display/spectrum/stop` | only when `display.spectrum.stop` is advertised |

### Error shape

JSON-RPC errors carry bounded `data` with a `kind` discriminator:

| kind | JSON-RPC code | Meaning |
|---|---|---|
| `invalidArguments` | `-32602` | local fail-closed rejection (bad type/missing/extra/out-of-range) — the device was never contacted |
| `unknownTool` | `-32602` | tool name not in the fixed catalog |
| `unknownMethod` | `-32601` | method not `ping`/`tools/list`/`tools/call` |
| `unsupportedCapability` | `-32000` | conditional tool whose capability is not advertised (fail-closed, no device call) |
| `deviceError` | `-32000` | the device's §9 typed error envelope (`code`, `message`, `available?`) |
| `deviceUnreachable` | `-32000` | connection/timeout failure; the service keeps running |
| `deviceProtocol` | `-32000` | malformed device response |

Parse/invalid-request errors use the reserved `-32700`/`-32600` codes.
Request lines are capped at 64 KiB (oversized lines are drained and answered
with a single `-32600`).

## Deterministic guarantees

- Arguments are validated locally and **fail-closed** before any device call;
  malformed input mutates nothing and never reaches the device.
- No adapter-side visual state: the device is authoritative; every mutating
  tool is a single validated HTTP command (single attempt — no retry storms).
- Capability eligibility is TTL-bounded (`capability_ttl_s`, default 5 s,
  clamped 1..300) and **fails closed**: if the identity refresh fails,
  conditional tools disappear until a refresh succeeds; core tools are
  unaffected (always advertised per §10). Discovery never implies
  authorization — control is the same fixed tool set for every client.
- Single-threaded sequential loop; fixed-size caches; bounded strings
  everywhere; single monotonic timebase for TTL decisions.

## Honest limitations

- No live HUGO-runtime integration has been performed (none exists in this
  repository); the surface is verified by deterministic fake-device host
  tests only. A HUGO host wanting a different transport must bridge it.
- Frozen scene/animation/expression vocabularies are embedded in tool
  descriptions (the API does not enumerate registries); device-side registry
  additions require an adapter update (LED-L005 precedent).
- v1 LAN-trust residual (no authentication) is inherited from the device API
  and documented in `docs/api/CONTRACT.md`.
