# LED Wall — Home Assistant External Adapter (LED-L005)

An **external adapter** (frozen spec §17, ADR-015) that exposes LED Wall to
Home Assistant over MQTT while talking to the device exclusively through the
semantic HTTP API (`docs/api/CONTRACT.md`). **MQTT never enters the firmware**;
this adapter is host-side Python 3 (standard library only, no pip installs)
and the ESP32 firmware has zero changes.

```text
Home Assistant ──MQTT──▶ broker ◀──MQTT── ha_adapter ◀──HTTP──▶ LED Wall device
```

## What it exposes in Home Assistant

| HA entity | Device command | Semantics |
|---|---|---|
| `light` (on/off + brightness, scale 0–100) | `display.brightness.set` | Brightness is the light-like entity (ADR-015). **On/off = drive level**: off → level 0 (dark, content preserved), on → restore last seen nonzero level (heritage default 40). The device has no true power-off. |
| `select` — scene (8 frozen keys) | `display.scene.set` | Frozen §7 registry (`corners, checker, border, row_sweep, bounce, wave, rain, plasma`). |
| `select` — animation (same 8 keys) | `display.animation.set` | Same frozen §7 keys, animation class. |
| `select` — expression (8 intents) | `display.expression.set` | Frozen §8 vocabulary (`idle, listening, thinking, speaking, happy, alert, error, sleeping`) — the alert/expression action surface. |
| `text` (≤128 chars) | `display.text.show` | Persistent text baseMode (no `durationMs`). |
| `button` — clear | `display.clear` | §11 CLEAR-1: cancel transient, blank, idle. |
| `sensor` — mode (+attributes) | `display.state.read` | `currentMode` with attributes (baseMode, active programs, transient, brightness, text, uptime, warnings). |
| `binary_sensor` — available | polling | Device reachability. |

Color/spectrum controls are **capability-gated**: created only if the device
advertises `color`/`spectrum` in `capabilities[]` (currently omitted, §5/§14/ADR-012).
The device is **not** forced into a single light entity (gate: "HA via external
adapter only; no forced single light entity").

## Setup

1. Copy `config.example.json` to `config.json` (git-ignored — put your real
   broker credentials there) and fill in:
   - `device_url`: the LED Wall API base, e.g. `http://led-wall.local` (plain HTTP in v1);
   - `mqtt_host` / `mqtt_port` / optional `mqtt_username` / `mqtt_password`;
   - optionally `discovery_enabled` (default `true`), `topic_prefix` (default `ledwall`),
     `poll_interval_s` (default 5, clamped 1–60), `keepalive_s` (default 30),
     `log_level`.
2. Run: `python3 -m ha_adapter --config /path/to/config.json` (Python ≥ 3.8).
3. In Home Assistant, add/integrate your MQTT broker; entities appear via MQTT
   discovery (`homeassistant/...`). Discovery can be disabled in the config; the
   device itself performs no self-advertising — the adapter requires explicit
   configuration of device and broker (see the discovery boundary note in
   `docs/decisions/ADR-LED-L005-HA-EXTERNAL-ADAPTER.md`).

## MQTT topic layout (one device)

```
<topic_prefix>/<deviceId>/availability            adapter process (birth/LWT: online/offline, retained)
<topic_prefix>/<deviceId>/device_availability    device reachability (retained)
<topic_prefix>/<deviceId>/light/{state,command}  JSON {"state":"ON|OFF","brightness":0..100}
<topic_prefix>/<deviceId>/scene/{state,command}  plain string, one of the frozen keys
<topic_prefix>/<deviceId>/animation/{state,command}
<topic_prefix>/<deviceId>/expression/{state,command}
<topic_prefix>/<deviceId>/text/{state,command}   plain string ≤128 chars
<topic_prefix>/<deviceId>/clear/command          any non-empty payload
<topic_prefix>/<deviceId>/mode/{state,attributes}
homeassistant/<component>/<deviceId>/<object_id>/config   discovery (retained)
```

State messages are retained QoS 1; commands are subscribed at QoS 1 with
`clean_session=false` and a stable client id (`ledwall-ha-<deviceId>`), so the
broker queues commands while the adapter restarts. Every entity is available
only when **both** the adapter process is running and the device answers.

## Honest behavior and limitations

- **Polling freshness:** device state is polled (default every 5 s; an
  accepted command triggers an immediate rate-limited re-poll). Short
  transients can be missed between polls; the WebSocket event channel is not
  used by adapter v1 (documented ADR decision, same polling-fallback pattern
  the accepted dashboard uses).
- **Frozen vocabularies embedded:** the API does not enumerate registries, so
  the adapter embeds the frozen §7/§8 sets; device-side registry additions
  would require an adapter update. Unknown names sent directly over MQTT are
  forwarded and the device answers with its typed error (logged, no mutation).
- **No QoS 2, no TLS** in adapter v1 (LAN-trust posture documented in the
  frozen spec §9); the adapter and broker must live on a trusted network.
- **No authentication beyond broker credentials** (v1 API has none — §9).
- Live behavior against a real broker/Home Assistant/device is **NOT
  verified** (V3/V4+ pending infrastructure and device authorization); it is
  verified at code level by the deterministic in-process integration suite
  (`tests/test_adapter_integration.py`) over a fake broker and fake device.
