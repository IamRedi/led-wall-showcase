"""Pure entity mapping between LED Wall API semantics and MQTT/HA payloads.

No I/O here: everything is a pure function over the normalized device state,
the device identity, and raw MQTT payloads, so the ADR-015 mapping table is
directly unit-testable. Topic layout lives in :class:`Topics`.

Vocabularies are transcribed from the frozen spec (§7 registry — the 8
animation-class keys are usable as scenes; §8 expression vocabulary — 8 frozen
intents including ``alert``). The API does not enumerate registries, so the
adapter embeds the frozen sets (documented limitation; device remains
authoritative and answers unknown names with typed errors).

On/off semantics (ADR-LED-L005 decision 4): the device has no power-off.
"off" = drive level 0 (content preserved, dark); "on" = restore the last seen
nonzero level (heritage default 40 if none seen). No device state is
resurrected — the restore level is adapter-side control input only.
"""

from __future__ import annotations

import json
from dataclasses import dataclass
from typing import Any, Dict, Optional, Tuple

from .device import DeviceIdentity, DeviceState

# Frozen spec §7 — initial registry: only animation-class entries exist, and
# "animations = motion effects usable as scenes"; scene.set accepts these keys.
FROZEN_SCENE_ANIMATION_KEYS: Tuple[str, ...] = (
    "corners", "checker", "border", "row_sweep", "bounce", "wave", "rain", "plasma",
)
# Frozen spec §8 — expression vocabulary (expression intent includes `alert`).
FROZEN_EXPRESSIONS: Tuple[str, ...] = (
    "idle", "listening", "thinking", "speaking", "happy", "alert", "error", "sleeping",
)

TEXT_MAX_CHARS = 128  # frozen spec §6 protocol cap
SELECT_NAME_MAX_CHARS = 64  # local sanity bound; device stays authoritative
BRIGHTNESS_MIN = 0
BRIGHTNESS_MAX = 100  # brightness_scale 100
HERITAGE_BRIGHTNESS_DEFAULT = 40  # frozen spec §2 brightness heritage

# Command payload bound: text allows 128 characters = up to 512 bytes as
# UTF-8 worst case; 1024 bounds all command kinds with headroom.
_COMMAND_PAYLOAD_MAX_BYTES = 1024


class Topics:
    """MQTT topic layout for one device (bounded, fixed set of topics)."""

    def __init__(self, topic_prefix: str, device_id: str):
        base = f"{topic_prefix}/{device_id}"
        self.availability = f"{base}/availability"  # adapter process LWT/birth
        self.device_availability = f"{base}/device_availability"  # device reachability
        self.light_state = f"{base}/light/state"
        self.light_command = f"{base}/light/command"
        self.scene_state = f"{base}/scene/state"
        self.scene_command = f"{base}/scene/command"
        self.animation_state = f"{base}/animation/state"
        self.animation_command = f"{base}/animation/command"
        self.expression_state = f"{base}/expression/state"
        self.expression_command = f"{base}/expression/command"
        self.text_state = f"{base}/text/state"
        self.text_command = f"{base}/text/command"
        self.clear_command = f"{base}/clear/command"
        self.mode_state = f"{base}/mode/state"
        self.mode_attributes = f"{base}/mode/attributes"


@dataclass(frozen=True)
class ApiCommand:
    """A §5 command the adapter decided to forward to the device."""

    path: str
    body: Dict[str, Any]


@dataclass(frozen=True)
class EntitySet:
    """Which entities exist for this device (capability-gated)."""

    light: bool
    scene: bool
    animation: bool
    expression: bool
    text: bool
    clear_button: bool
    mode_sensor: bool
    available_sensor: bool

    def command_topics(self, topics: Topics) -> Tuple[Tuple[str, str], ...]:
        """(topic, kind) pairs to subscribe for commands, for existing entities only."""
        pairs: list = []
        if self.light:
            pairs.append((topics.light_command, "light"))
        if self.scene:
            pairs.append((topics.scene_command, "scene"))
        if self.animation:
            pairs.append((topics.animation_command, "animation"))
        if self.expression:
            pairs.append((topics.expression_command, "expression"))
        if self.text:
            pairs.append((topics.text_command, "text"))
        if self.clear_button:
            pairs.append((topics.clear_command, "clear"))
        return tuple(pairs)


def entity_set_for(identity: DeviceIdentity) -> EntitySet:
    """Capability-gated entity set: the §5 core set is always advertised."""
    caps = set(identity.capabilities)

    def has(cap: str) -> bool:
        return cap in caps

    return EntitySet(
        light=has("display.brightness.set"),
        scene=has("display.scene.set"),
        animation=has("display.animation.set"),
        expression=has("display.expression.set"),
        text=has("display.text.show"),
        clear_button=has("display.clear"),
        mode_sensor=has("display.state.read"),
        available_sensor=has("display.state.read"),
    )


def _device_block(identity: DeviceIdentity) -> Dict[str, Any]:
    return {
        "identifiers": [identity.device_id],
        "name": "LED Wall",
        "manufacturer": "Redion Cekrezi",
        "model": "LED Wall v1 (visual_display_v1)",
        "sw_version": identity.fw_version,
    }


def _availability(topics: Topics) -> Tuple[Dict[str, str], ...]:
    # Entity is available only when the adapter runs AND the device answers.
    return (
        {"topic": topics.availability},
        {"topic": topics.device_availability},
    )


def _base_config(unique_suffix: str, object_id: str, name: str) -> Dict[str, Any]:
    return {
        "unique_id": f"led_wall_{unique_suffix}",
        "object_id": object_id,
        "name": name,
        "qos": 1,
        "retain": True,
    }


def build_discovery_configs(
    identity: DeviceIdentity, topics: Topics, discovery_prefix: str
) -> Dict[str, Dict[str, Any]]:
    """HA MQTT discovery payloads keyed by their config topic (retained)."""
    entity_set = entity_set_for(identity)
    configs: Dict[str, Dict[str, Any]] = {}
    dev = _device_block(identity)
    avail = _availability(topics)

    def add(component: str, object_id: str, name: str, payload: Dict[str, Any]) -> None:
        topic = f"{discovery_prefix}/{component}/{identity.device_id}/{object_id}/config"
        payload = dict(payload)
        payload.update(_base_config(f"{identity.device_id}_{object_id}", object_id, name))
        payload["availability"] = [dict(a) for a in avail]
        payload["availability_mode"] = "all"
        payload["device"] = dev
        configs[topic] = payload

    if entity_set.light:
        add(
            "light",
            "display",
            "LED Wall display",
            {
                "schema": "default",
                "state_topic": topics.light_state,
                "command_topic": topics.light_command,
                "brightness_scale": BRIGHTNESS_MAX,
                "optimistic": False,
            },
        )
    if entity_set.scene:
        add(
            "select",
            "scene",
            "LED Wall scene",
            {
                "options": list(FROZEN_SCENE_ANIMATION_KEYS),
                "state_topic": topics.scene_state,
                "command_topic": topics.scene_command,
            },
        )
    if entity_set.animation:
        add(
            "select",
            "animation",
            "LED Wall animation",
            {
                "options": list(FROZEN_SCENE_ANIMATION_KEYS),
                "state_topic": topics.animation_state,
                "command_topic": topics.animation_command,
            },
        )
    if entity_set.expression:
        add(
            "select",
            "expression",
            "LED Wall expression",
            {
                "options": list(FROZEN_EXPRESSIONS),
                "state_topic": topics.expression_state,
                "command_topic": topics.expression_command,
            },
        )
    if entity_set.text:
        add(
            "text",
            "text",
            "LED Wall text",
            {
                "state_topic": topics.text_state,
                "command_topic": topics.text_command,
                "min": 0,
                "max": TEXT_MAX_CHARS,
                "mode": "text",
            },
        )
    if entity_set.clear_button:
        add(
            "button",
            "clear",
            "LED Wall clear",
            {
                "command_topic": topics.clear_command,
                "payload_press": "press",
            },
        )
    if entity_set.mode_sensor:
        add(
            "sensor",
            "mode",
            "LED Wall mode",
            {
                "state_topic": topics.mode_state,
                "json_attributes_topic": topics.mode_attributes,
                "icon": "mdi:led-strip",
            },
        )
    if entity_set.available_sensor:
        add(
            "binary_sensor",
            "available",
            "LED Wall available",
            {
                "state_topic": topics.device_availability,
                "payload_on": "online",
                "payload_off": "offline",
                "icon": "mdi:connection",
            },
        )
    return configs


def _json(payload: Dict[str, Any]) -> str:
    return json.dumps(payload, separators=(",", ":"), ensure_ascii=True)


def build_state_payloads(
    state: DeviceState, restore_level: Optional[int]
) -> Dict[str, str]:
    """Map a normalized device state to (topic -> payload) MQTT state messages."""
    payloads: Dict[str, str] = {}

    # Defensive 0..100 clamp: the semantic range is 0..100 regardless of what
    # a misbehaving peer reports; HA receives brightness on this scale.
    level = max(BRIGHTNESS_MIN, min(BRIGHTNESS_MAX, state.brightness_current))
    light: Dict[str, Any] = {"state": "ON" if level > 0 else "OFF"}
    if level > 0:
        light["brightness"] = level
    payloads["light"] = _json(light)

    payloads["scene"] = state.active_scene if state.base_mode == "scene" else ""
    payloads["animation"] = state.active_animation if state.base_mode == "animation" else ""
    payloads["expression"] = state.active_expression if state.base_mode == "expression" else ""
    payloads["text"] = state.text_current if state.text_current is not None else ""
    payloads["mode"] = state.current_mode
    payloads["attributes"] = _json(
        {
            "baseMode": state.base_mode,
            "activeScene": state.active_scene,
            "activeAnimation": state.active_animation,
            "activeExpression": state.active_expression,
            "transientKind": state.transient_kind,
            "transientRemainingMs": state.transient_remaining_ms,
            "brightnessCurrent": state.brightness_current,
            "brightnessMax": state.brightness_max,
            "textCurrent": state.text_current,
            "textScrolling": state.text_scrolling,
            "uptimeSeconds": state.uptime_seconds,
            "warnings": list(state.warnings),
        }
    )
    # `restore_level` (adapter-side control input for the on-toggle) is
    # deliberately NOT published anywhere: it is never presented as device state.
    del restore_level
    return payloads


def state_topic_map(topics: Topics) -> Dict[str, str]:
    """key (as used by build_state_payloads) -> concrete MQTT topic."""
    return {
        "light": topics.light_state,
        "scene": topics.scene_state,
        "animation": topics.animation_state,
        "expression": topics.expression_state,
        "text": topics.text_state,
        "mode": topics.mode_state,
        "attributes": topics.mode_attributes,
    }


def parse_command(
    kind: str, payload: bytes, restore_level: Optional[int]
) -> Optional[ApiCommand]:
    """Translate an MQTT command payload into a §5 API command.

    Returns None (and the caller logs) for anything invalid — invalid input
    never reaches the device and never mutates anything. `restore_level` is
    the adapter-side last-seen nonzero brightness used by light "ON" without
    an explicit brightness (heritage default when None).
    """
    if len(payload) > _COMMAND_PAYLOAD_MAX_BYTES:
        return None
    try:
        text = payload.decode("utf-8")
    except UnicodeDecodeError:
        return None

    if kind == "light":
        return _parse_light_command(text, restore_level)
    if kind in ("scene", "animation", "expression"):
        return _parse_name_command(kind, text)
    if kind == "text":
        if not (1 <= len(text) <= TEXT_MAX_CHARS):
            return None
        return ApiCommand("/display/text/show", {"text": text})
    if kind == "clear":
        if not text.strip():
            return None
        return ApiCommand("/display/clear", {})
    return None


def _parse_light_command(text: str, restore_level: Optional[int]) -> Optional[ApiCommand]:
    try:
        data = json.loads(text)
    except json.JSONDecodeError:
        return None
    if not isinstance(data, dict):
        return None
    raw_state = data.get("state")
    if isinstance(raw_state, bool):
        state = "ON" if raw_state else "OFF"
    elif isinstance(raw_state, str):
        state = raw_state.strip().upper()
        if state not in ("ON", "OFF"):
            return None
    else:
        return None

    raw_brightness = data.get("brightness")
    if raw_brightness is None:
        brightness: Optional[int] = None
    elif isinstance(raw_brightness, bool) or not isinstance(raw_brightness, (int, float)):
        return None
    else:
        brightness = int(raw_brightness)
        if brightness < BRIGHTNESS_MIN or brightness > BRIGHTNESS_MAX:
            return None

    if state == "OFF":
        return ApiCommand("/display/brightness/set", {"level": BRIGHTNESS_MIN})
    if brightness is not None:
        return ApiCommand("/display/brightness/set", {"level": brightness})
    level = restore_level if restore_level is not None else HERITAGE_BRIGHTNESS_DEFAULT
    level = max(BRIGHTNESS_MIN, min(BRIGHTNESS_MAX, level))
    return ApiCommand("/display/brightness/set", {"level": level})


def _parse_name_command(kind: str, text: str) -> Optional[ApiCommand]:
    name = text.strip()
    if not name or len(name) > SELECT_NAME_MAX_CHARS:
        return None
    if any(ord(ch) < 0x20 for ch in name):
        return None
    if kind == "scene":
        return ApiCommand("/display/scene/set", {"scene": name})
    if kind == "animation":
        return ApiCommand("/display/animation/set", {"animation": name})
    return ApiCommand("/display/expression/set", {"expression": name})
