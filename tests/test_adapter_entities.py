"""Entity mapping tests (C-L005-1/C-L005-7): ADR-015 mapping table, discovery
shapes, and command parsing.

The vocabularies below are transcribed independently from the frozen spec
(§7 registry table; §8 expression contract) and cross-checked against
docs/api/CONTRACT.md — not copied from the implementation.
"""

import json
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from ha_adapter.device import DeviceIdentity, DeviceState  # noqa: E402
from ha_adapter.entities import (  # noqa: E402
    BRIGHTNESS_MAX,
    FROZEN_EXPRESSIONS,
    FROZEN_SCENE_ANIMATION_KEYS,
    HERITAGE_BRIGHTNESS_DEFAULT,
    TEXT_MAX_CHARS,
    Topics,
    build_discovery_configs,
    build_state_payloads,
    entity_set_for,
    parse_command,
    state_topic_map,
)

# Independently transcribed from the frozen specification:
SPEC_SECTION_7_REGISTRY = ("corners", "checker", "border", "row_sweep", "bounce", "wave", "rain", "plasma")
SPEC_SECTION_8_EXPRESSIONS = ("idle", "listening", "thinking", "speaking", "happy", "alert", "error", "sleeping")

DEVICE_ID = "led-wall-ab12cd34"
IDENTITY = DeviceIdentity(
    device_id=DEVICE_ID, device_type="visual_display_v1", fw_version="0.1.0-test",
    capabilities=(
        "display.text.show", "display.text.clear", "display.scene.set",
        "display.animation.set", "display.expression.set", "display.clear",
        "display.brightness.set", "display.state.read",
    ),
)
TOPICS = Topics("ledwall", DEVICE_ID)


def make_state(**overrides):
    fields = dict(
        available=True, device_id=DEVICE_ID, base_mode="idle", current_mode="idle",
        active_scene=None, active_animation=None, active_expression=None,
        transient_kind=None, transient_remaining_ms=None, text_current=None,
        text_scrolling=False, brightness_current=40, brightness_max=80,
        uptime_seconds=90, warnings=(),
    )
    fields.update(overrides)
    return DeviceState(**fields)


class VocabularyTest(unittest.TestCase):
    def test_module_vocabularies_equal_spec(self):
        self.assertEqual(FROZEN_SCENE_ANIMATION_KEYS, SPEC_SECTION_7_REGISTRY)
        self.assertEqual(FROZEN_EXPRESSIONS, SPEC_SECTION_8_EXPRESSIONS)
        self.assertEqual(TEXT_MAX_CHARS, 128)  # frozen §6 protocol cap
        self.assertEqual(BRIGHTNESS_MAX, 100)  # frozen §13 semantic range
        self.assertEqual(HERITAGE_BRIGHTNESS_DEFAULT, 40)  # frozen §2 heritage


class EntitySetTest(unittest.TestCase):
    def test_core_capabilities_enable_all_entities(self):
        entity_set = entity_set_for(IDENTITY)
        self.assertTrue(entity_set.light)
        self.assertTrue(entity_set.scene)
        self.assertTrue(entity_set.animation)
        self.assertTrue(entity_set.expression)
        self.assertTrue(entity_set.text)
        self.assertTrue(entity_set.clear_button)
        self.assertTrue(entity_set.mode_sensor)
        self.assertTrue(entity_set.available_sensor)

    def test_missing_capability_disables_entity(self):
        caps = [c for c in IDENTITY.capabilities if c != "display.scene.set"]
        identity = DeviceIdentity(DEVICE_ID, "visual_display_v1", "0.1.0-test", tuple(caps))
        self.assertFalse(entity_set_for(identity).scene)
        self.assertTrue(entity_set_for(identity).light)

    def test_command_topics_match_existing_entities(self):
        topics = entity_set_for(IDENTITY).command_topics(TOPICS)
        kinds = {kind for _topic, kind in topics}
        self.assertEqual(
            kinds, {"light", "scene", "animation", "expression", "text", "clear"}
        )
        by_kind = {kind: topic for topic, kind in topics}
        self.assertEqual(by_kind["light"], f"ledwall/{DEVICE_ID}/light/command")
        self.assertEqual(by_kind["clear"], f"ledwall/{DEVICE_ID}/clear/command")


class DiscoveryTest(unittest.TestCase):
    def test_discovery_topic_and_unique_id_layout(self):
        configs = build_discovery_configs(IDENTITY, TOPICS, "homeassistant")
        self.assertEqual(
            sorted(configs.keys()),
            sorted(
                f"homeassistant/{component}/{DEVICE_ID}/{object_id}/config"
                for component, object_id in (
                    ("light", "display"), ("select", "scene"), ("select", "animation"),
                    ("select", "expression"), ("text", "text"), ("button", "clear"),
                    ("sensor", "mode"), ("binary_sensor", "available"),
                )
            ),
        )
        unique_ids = {payload["unique_id"] for payload in configs.values()}
        self.assertEqual(
            unique_ids,
            {f"led_wall_{DEVICE_ID}_{object_id}" for _c, object_id in (
                ("light", "display"), ("select", "scene"), ("select", "animation"),
                ("select", "expression"), ("text", "text"), ("button", "clear"),
                ("sensor", "mode"), ("binary_sensor", "available"),
            )},
        )
        for topic, payload in configs.items():
            with self.subTest(topic=topic):
                self.assertEqual(payload["qos"], 1)
                self.assertTrue(payload["retain"])
                self.assertEqual(payload["availability_mode"], "all")
                availability_topics = [a["topic"] for a in payload["availability"]]
                self.assertEqual(
                    availability_topics,
                    [TOPICS.availability, TOPICS.device_availability],
                )
                self.assertEqual(payload["device"]["identifiers"], [DEVICE_ID])

    def test_light_config(self):
        configs = build_discovery_configs(IDENTITY, TOPICS, "homeassistant")
        light = configs[f"homeassistant/light/{DEVICE_ID}/display/config"]
        self.assertEqual(light["schema"], "default")
        self.assertEqual(light["brightness_scale"], 100)
        self.assertEqual(light["state_topic"], TOPICS.light_state)
        self.assertEqual(light["command_topic"], TOPICS.light_command)
        self.assertFalse(light["optimistic"])

    def test_select_configs_mirror_frozen_vocabularies(self):
        configs = build_discovery_configs(IDENTITY, TOPICS, "homeassistant")
        scene = configs[f"homeassistant/select/{DEVICE_ID}/scene/config"]
        animation = configs[f"homeassistant/select/{DEVICE_ID}/animation/config"]
        expression = configs[f"homeassistant/select/{DEVICE_ID}/expression/config"]
        self.assertEqual(scene["options"], list(SPEC_SECTION_7_REGISTRY))
        self.assertEqual(animation["options"], list(SPEC_SECTION_7_REGISTRY))
        self.assertEqual(expression["options"], list(SPEC_SECTION_8_EXPRESSIONS))
        self.assertEqual(scene["state_topic"], TOPICS.scene_state)
        self.assertEqual(scene["command_topic"], TOPICS.scene_command)

    def test_text_config_bounded(self):
        configs = build_discovery_configs(IDENTITY, TOPICS, "homeassistant")
        text = configs[f"homeassistant/text/{DEVICE_ID}/text/config"]
        self.assertEqual(text["max"], 128)
        self.assertEqual(text["min"], 0)

    def test_sensor_configs(self):
        configs = build_discovery_configs(IDENTITY, TOPICS, "homeassistant")
        mode = configs[f"homeassistant/sensor/{DEVICE_ID}/mode/config"]
        self.assertEqual(mode["state_topic"], TOPICS.mode_state)
        self.assertEqual(mode["json_attributes_topic"], TOPICS.mode_attributes)
        binary = configs[f"homeassistant/binary_sensor/{DEVICE_ID}/available/config"]
        self.assertEqual(binary["state_topic"], TOPICS.device_availability)
        self.assertEqual(binary["payload_on"], "online")
        self.assertEqual(binary["payload_off"], "offline")

    def test_capability_gating_removes_configs(self):
        caps = [c for c in IDENTITY.capabilities
                if c not in ("display.scene.set", "display.expression.set")]
        identity = DeviceIdentity(DEVICE_ID, "visual_display_v1", "0.1.0-test", tuple(caps))
        configs = build_discovery_configs(identity, TOPICS, "homeassistant")
        self.assertNotIn(f"homeassistant/select/{DEVICE_ID}/scene/config", configs)
        self.assertNotIn(f"homeassistant/select/{DEVICE_ID}/expression/config", configs)
        self.assertIn(f"homeassistant/light/{DEVICE_ID}/display/config", configs)


class StatePayloadTest(unittest.TestCase):
    def test_topic_map_keys_cover_payloads(self):
        payloads = build_state_payloads(make_state(), None)
        self.assertEqual(set(payloads), set(state_topic_map(TOPICS)))

    def test_light_on_with_brightness(self):
        payloads = build_state_payloads(make_state(brightness_current=57), None)
        self.assertEqual(json.loads(payloads["light"]), {"state": "ON", "brightness": 57})

    def test_light_off_at_zero(self):
        payloads = build_state_payloads(make_state(brightness_current=0), None)
        self.assertEqual(json.loads(payloads["light"]), {"state": "OFF"})

    def test_light_brightness_boundaries(self):
        payloads = build_state_payloads(make_state(brightness_current=100), None)
        self.assertEqual(json.loads(payloads["light"])["brightness"], 100)
        payloads = build_state_payloads(make_state(brightness_current=1), None)
        self.assertEqual(json.loads(payloads["light"])["brightness"], 1)

    def test_light_clamps_rogue_brightness_reports(self):
        payloads = build_state_payloads(make_state(brightness_current=5000), None)
        self.assertEqual(json.loads(payloads["light"]),
                         {"state": "ON", "brightness": 100})

    def test_select_states_follow_base_mode(self):
        state = make_state(base_mode="scene", current_mode="scene", active_scene="wave",
                           active_animation="rain", active_expression="happy")
        payloads = build_state_payloads(state, None)
        self.assertEqual(payloads["scene"], "wave")
        self.assertEqual(payloads["animation"], "")  # base slot only (O-L003-3 semantics)
        self.assertEqual(payloads["expression"], "")

    def test_text_state_when_absent(self):
        payloads = build_state_payloads(make_state(), None)
        self.assertEqual(payloads["text"], "")

    def test_mode_and_attributes(self):
        state = make_state(
            base_mode="scene", current_mode="animation", active_scene="wave",
            transient_kind="animation", transient_remaining_ms=1500,
            text_current=None, warnings=("w1",),
        )
        payloads = build_state_payloads(state, None)
        self.assertEqual(payloads["mode"], "animation")
        attributes = json.loads(payloads["attributes"])
        self.assertEqual(attributes["baseMode"], "scene")
        self.assertEqual(attributes["transientRemainingMs"], 1500)
        self.assertEqual(attributes["activeScene"], "wave")
        self.assertEqual(attributes["warnings"], ["w1"])

    def test_restore_level_is_never_published(self):
        payloads = build_state_payloads(make_state(), 55)
        for payload in payloads.values():
            self.assertNotIn("55", payload)


class ParseCommandTest(unittest.TestCase):
    def test_light_on_with_brightness(self):
        cmd = parse_command("light", b'{"state":"ON","brightness":42}', None)
        self.assertEqual((cmd.path, cmd.body), ("/display/brightness/set", {"level": 42}))

    def test_light_on_without_brightness_restores(self):
        cmd = parse_command("light", b'{"state":"ON"}', 55)
        self.assertEqual((cmd.path, cmd.body), ("/display/brightness/set", {"level": 55}))

    def test_light_on_without_history_uses_heritage_default(self):
        cmd = parse_command("light", b'{"state":"ON"}', None)
        self.assertEqual(cmd.body, {"level": 40})

    def test_light_off_maps_to_level_zero(self):
        cmd = parse_command("light", b'{"state":"OFF"}', 55)
        self.assertEqual((cmd.path, cmd.body), ("/display/brightness/set", {"level": 0}))

    def test_light_state_is_case_and_space_tolerant(self):
        self.assertEqual(parse_command("light", b'{"state":" on "}',
                                       55).body, {"level": 55})

    def test_light_boolean_state_accepted(self):
        self.assertEqual(parse_command("light", b'{"state":true}', None).body, {"level": 40})
        self.assertEqual(parse_command("light", b'{"state":false}', None).body, {"level": 0})

    def test_light_brightness_rejects_invalid_values(self):
        for payload in (
            b'{"state":"ON","brightness":101}',
            b'{"state":"ON","brightness":-1}',
            b'{"state":"ON","brightness":"abc"}',
            b'{"state":"ON","brightness":true}',
        ):
            with self.subTest(payload=payload):
                self.assertIsNone(parse_command("light", payload, None))

    def test_light_rejects_malformed_payloads(self):
        for payload in (b"not json", b"[1]", b'{"state":"MAYBE"}', b"{}"):
            with self.subTest(payload=payload):
                self.assertIsNone(parse_command("light", payload, None))

    def test_light_brightness_boundaries_accepted(self):
        self.assertEqual(parse_command("light", b'{"state":"ON","brightness":0}',
                                       None).body, {"level": 0})
        self.assertEqual(parse_command("light", b'{"state":"ON","brightness":100}',
                                       None).body, {"level": 100})

    def test_scene_animation_expression_forwarding(self):
        for kind, path, key in (
            ("scene", "/display/scene/set", "scene"),
            ("animation", "/display/animation/set", "animation"),
            ("expression", "/display/expression/set", "expression"),
        ):
            with self.subTest(kind=kind):
                cmd = parse_command(kind, b"wave", None)
                self.assertEqual((cmd.path, cmd.body), (path, {key: "wave"}))

    def test_name_commands_local_sanity_bounds(self):
        self.assertIsNone(parse_command("scene", b"", None))
        self.assertIsNone(parse_command("scene", b"   ", None))
        self.assertIsNone(parse_command("scene", b"x" * 65, None))
        self.assertIsNone(parse_command("scene", b"a\x00b", None))
        self.assertIsNone(parse_command("scene", b"a\nb", None))
        # 64 chars is exactly at the local sanity bound.
        self.assertIsNotNone(parse_command("scene", b"x" * 64, None))
        # Registry names are NOT locally pre-validated: the device is
        # authoritative and answers unknown names with typed errors.
        self.assertEqual(parse_command("scene", b"not-a-scene", None).body, {"scene": "not-a-scene"})

    def test_text_forwarding_with_protocol_cap(self):
        cmd = parse_command("text", b"HELLO", None)
        self.assertEqual((cmd.path, cmd.body), ("/display/text/show", {"text": "HELLO"}))
        self.assertIsNotNone(parse_command("text", b"x" * 128, None))
        self.assertIsNone(parse_command("text", b"x" * 129, None))
        self.assertIsNone(parse_command("text", b"", None))

    def test_text_is_not_mutated(self):
        cmd = parse_command("text", b"  HELLO  ", None)
        self.assertEqual(cmd.body, {"text": "  HELLO  "})

    def test_clear_button(self):
        cmd = parse_command("clear", b"press", None)
        self.assertEqual((cmd.path, cmd.body), ("/display/clear", {}))
        self.assertIsNone(parse_command("clear", b"", None))
        self.assertIsNone(parse_command("clear", b"   ", None))

    def test_non_utf8_and_oversized_payloads_rejected(self):
        self.assertIsNone(parse_command("scene", b"\xff\xfe\xfd", None))
        self.assertIsNone(parse_command("text", b"x" * 1025, None))
        self.assertIsNone(parse_command("unknown-kind", b"x", None))


if __name__ == "__main__":
    unittest.main()
