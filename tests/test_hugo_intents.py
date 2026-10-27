"""HUGO DeviceAdapter intent-catalog tests (C-L006-1/C-L006-7): the frozen
spec §16 boundary made checkable.

The allowed tool surface, vocabularies, and bounds are transcribed
independently from the frozen spec text (§5/§6/§7/§8/§16) and
docs/api/CONTRACT.md — not from the implementation constants.
"""

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from hugo_adapter import intents  # noqa: E402
from hugo_adapter.intents import InvalidArguments, build_request, tool_by_name  # noqa: E402

# §16: "HUGO/LOOI may use: text, scene, animation, expression, brightness,
# clear, state (plus spectrum/color where capability-flagged)" — transcribed
# to the concrete intent tools those verbs cover on the v1 API.
SPEC16_ALLOWED_TOOLS = {
    # text
    "show_text", "clear_text",
    # scene / animation
    "set_scene", "set_animation",
    # expression
    "set_expression",
    # brightness
    "set_brightness",
    # clear
    "clear_display",
    # state
    "get_state",
    # identity/capabilities are the §10 handshake surface every client reads
    "get_device_info",
    # spectrum/color where capability-flagged (§16 conditional clause)
    "set_color", "start_spectrum", "stop_spectrum",
}

# CONTRACT.md command endpoints + queries (independently transcribed).
DOCUMENTED_ENDPOINTS = {
    ("POST", "/display/text/show"),
    ("POST", "/display/text/clear"),
    ("POST", "/display/scene/set"),
    ("POST", "/display/animation/set"),
    ("POST", "/display/expression/set"),
    ("POST", "/display/clear"),
    ("POST", "/display/brightness/set"),
    ("GET", "/state"),
    ("GET", "/identity"),
    # Conditional §5 capabilities (documented shape; currently unadvertised).
    ("POST", "/display/color/set"),
    ("POST", "/display/spectrum/start"),
    ("POST", "/display/spectrum/stop"),
}

# Frozen §7 registry (transcribed from the spec's legacy→semantic table).
SPEC_SCENE_KEYS = ("corners", "checker", "border", "row_sweep", "bounce", "wave", "rain", "plasma")
SPEC_ANIMATION_KEYS = ("corners", "checker", "border", "row_sweep", "bounce", "wave", "rain", "plasma")
# Frozen §8 vocabulary (transcribed).
SPEC_EXPRESSION_KEYS = ("idle", "listening", "thinking", "speaking", "happy", "alert", "error", "sleeping")
# §7 spectrum styles (transcribed from the preset rows).
SPEC_SPECTRUM_STYLES = (
    "spectrum.rows", "spectrum.columns", "spectrum.mirror", "spectrum.rainbow",
    "spectrum.peaks", "spectrum.center_out", "spectrum.blocks", "spectrum.fire",
)

RENDERER_INTERNAL_MARKERS = ("fastled", "gpio", "serpentine", "ledindex", "led_index", "pixel", "framebuffer", "renderer")


class CatalogBoundaryTest(unittest.TestCase):
    def test_catalog_is_exactly_the_spec16_surface(self):
        self.assertEqual(intents.ALLOWED_TOOL_NAMES, SPEC16_ALLOWED_TOOLS)

    def test_every_tool_maps_to_a_documented_endpoint(self):
        for spec in intents.TOOL_SPECS:
            method, path, _ = build_request(spec, _default_args(spec))
            self.assertIn((method, path), DOCUMENTED_ENDPOINTS, f"{spec.name} -> {(method, path)}")

    def test_no_tool_or_endpoint_exposes_renderer_internals(self):
        for spec in intents.TOOL_SPECS:
            blob = f"{spec.name} {spec.summary}".lower()
            for marker in RENDERER_INTERNAL_MARKERS:
                self.assertNotIn(marker, blob, f"tool {spec.name} mentions renderer internal {marker}")
            _, path, _ = build_request(spec, _default_args(spec))
            self.assertNotIn("paint", path)  # diagnostic paint is never a HUGO tool (§5)

    def test_vocabularies_match_spec_transcription(self):
        self.assertEqual(tuple(intents.SCENE_KEYS), SPEC_SCENE_KEYS)
        self.assertEqual(tuple(intents.ANIMATION_KEYS), SPEC_ANIMATION_KEYS)
        self.assertEqual(tuple(intents.EXPRESSION_KEYS), SPEC_EXPRESSION_KEYS)
        self.assertEqual(tuple(intents.SPECTRUM_STYLES), SPEC_SPECTRUM_STYLES)

    def test_expression_description_carries_full_frozen_vocabulary(self):
        spec = tool_by_name("set_expression")
        for intent in SPEC_EXPRESSION_KEYS:
            self.assertIn(intent, spec.summary)

    def test_core_tools_are_ungated_conditional_tools_are_gated(self):
        core_names = {s.name for s in intents.core_tools()}
        conditional = {s.name: s.capability for s in intents.conditional_tools()}
        self.assertEqual(
            core_names,
            SPEC16_ALLOWED_TOOLS - {"set_color", "start_spectrum", "stop_spectrum"},
        )
        self.assertEqual(
            conditional,
            {
                "set_color": "display.color.set",
                "start_spectrum": "display.spectrum.start",
                "stop_spectrum": "display.spectrum.stop",
            },
        )


class ListingGatingTest(unittest.TestCase):
    def test_unknown_capabilities_fail_closed(self):
        listed = intents.tool_listing(None)
        self.assertEqual({s.name for s in listed}, {s.name for s in intents.core_tools()})

    def test_empty_capabilities_fail_closed_for_conditional(self):
        listed = intents.tool_listing(())
        self.assertEqual({s.name for s in listed}, {s.name for s in intents.core_tools()})

    def test_advertised_capability_enables_its_tool(self):
        listed = intents.tool_listing(("display.color.set",))
        self.assertEqual({s.name for s in listed}, {s.name for s in intents.core_tools()} | {"set_color"})

    def test_partial_advertisement_only_enables_matching_tools(self):
        listed = intents.tool_listing(("display.spectrum.start",))  # start only, not stop
        names = {s.name for s in listed}
        self.assertIn("start_spectrum", names)
        self.assertNotIn("stop_spectrum", names)
        self.assertNotIn("set_color", names)


class ShowTextValidationTest(unittest.TestCase):
    def call(self, arguments):
        return build_request(tool_by_name("show_text"), arguments)

    def test_minimal(self):
        method, path, payload = self.call({"text": "HI"})
        self.assertEqual((method, path), ("POST", "/display/text/show"))
        self.assertEqual(payload, {"text": "HI"})

    def test_full_arguments(self):
        _, _, payload = self.call({"text": "X", "scroll": True, "speedMsPerPx": 90, "durationMs": 5000})
        self.assertEqual(payload, {"text": "X", "scroll": True, "speedMsPerPx": 90, "durationMs": 5000})

    def test_one_hundred_twenty_eight_chars_accepted(self):
        _, _, payload = self.call({"text": "A" * 128})
        self.assertEqual(payload["text"], "A" * 128)

    def test_empty_text_rejected(self):
        with self.assertRaises(InvalidArguments):
            self.call({"text": ""})

    def test_oversized_text_rejected(self):
        with self.assertRaises(InvalidArguments):
            self.call({"text": "A" * 129})

    def test_non_string_text_rejected(self):
        with self.assertRaises(InvalidArguments):
            self.call({"text": 42})

    def test_missing_text_rejected(self):
        with self.assertRaises(InvalidArguments):
            self.call({})

    def test_lone_surrogate_rejected(self):
        with self.assertRaises(InvalidArguments):
            self.call({"text": "bad\ud800surrogate"})

    def test_non_bool_scroll_rejected(self):
        with self.assertRaises(InvalidArguments):
            self.call({"text": "X", "scroll": "yes"})

    def test_speed_boundaries_accepted(self):
        for speed in (40, 140, 500):
            _, _, payload = self.call({"text": "X", "speedMsPerPx": speed})
            self.assertEqual(payload["speedMsPerPx"], speed)

    def test_speed_out_of_bounds_rejected(self):
        for speed in (39, 501, -40):
            with self.assertRaises(InvalidArguments):
                self.call({"text": "X", "speedMsPerPx": speed})

    def test_speed_bool_rejected(self):
        with self.assertRaises(InvalidArguments):
            self.call({"text": "X", "speedMsPerPx": True})

    def test_speed_float_rejected(self):
        with self.assertRaises(InvalidArguments):
            self.call({"text": "X", "speedMsPerPx": 140.0})

    def test_duration_boundaries(self):
        _, _, payload = self.call({"text": "X", "durationMs": 1})
        self.assertEqual(payload["durationMs"], 1)
        _, _, payload = self.call({"text": "X", "durationMs": 2147483647})
        self.assertEqual(payload["durationMs"], 2147483647)

    def test_duration_out_of_bounds_rejected(self):
        for duration in (0, -1, 2147483648):
            with self.assertRaises(InvalidArguments):
                self.call({"text": "X", "durationMs": duration})

    def test_unknown_argument_rejected(self):
        with self.assertRaises(InvalidArguments):
            self.call({"text": "X", "color": "red"})

    def test_non_object_arguments_rejected(self):
        for bad in ("text", 42, ["X"], True):
            with self.assertRaises(InvalidArguments):
                self.call(bad)


class NameToolValidationTest(unittest.TestCase):
    def test_scene_minimal_and_full(self):
        method, path, payload = build_request(tool_by_name("set_scene"), {"scene": "wave"})
        self.assertEqual((method, path), ("POST", "/display/scene/set"))
        self.assertEqual(payload, {"scene": "wave"})
        _, _, payload = build_request(tool_by_name("set_scene"), {"scene": "wave", "durationMs": 8000})
        self.assertEqual(payload, {"scene": "wave", "durationMs": 8000})

    def test_animation_minimal(self):
        method, path, payload = build_request(tool_by_name("set_animation"), {"animation": "plasma"})
        self.assertEqual((method, path), ("POST", "/display/animation/set"))
        self.assertEqual(payload, {"animation": "plasma"})

    def test_expression_minimal(self):
        method, path, payload = build_request(tool_by_name("set_expression"), {"expression": "thinking"})
        self.assertEqual((method, path), ("POST", "/display/expression/set"))
        self.assertEqual(payload, {"expression": "thinking"})

    def test_expression_duration_ms_rejected(self):
        # Expressions are persistent by design (§5): no transient form.
        with self.assertRaises(InvalidArguments):
            build_request(tool_by_name("set_expression"), {"expression": "alert", "durationMs": 1000})

    def test_unknown_names_are_forwarded_not_locally_filtered(self):
        # Registry membership is device-authoritative; the adapter forwards
        # sane names and surfaces the device's typed error + available list.
        for tool, key in (("set_scene", "scene"), ("set_animation", "animation"), ("set_expression", "expression")):
            _, _, payload = build_request(tool_by_name(tool), {key: "definitely_not_registered"})
            self.assertEqual(payload[key], "definitely_not_registered")

    def test_empty_name_rejected(self):
        for tool, key in (("set_scene", "scene"), ("set_animation", "animation"), ("set_expression", "expression")):
            with self.assertRaises(InvalidArguments):
                build_request(tool_by_name(tool), {key: ""})

    def test_oversize_name_rejected(self):
        for tool, key in (("set_scene", "scene"), ("set_animation", "animation"), ("set_expression", "expression")):
            with self.assertRaises(InvalidArguments):
                build_request(tool_by_name(tool), {key: "n" * 65})

    def test_control_character_name_rejected(self):
        for tool, key in (("set_scene", "scene"), ("set_animation", "animation"), ("set_expression", "expression")):
            with self.assertRaises(InvalidArguments):
                build_request(tool_by_name(tool), {key: "bad\nname"})

    def test_non_string_name_rejected(self):
        with self.assertRaises(InvalidArguments):
            build_request(tool_by_name("set_scene"), {"scene": 7})

    def test_missing_name_rejected(self):
        with self.assertRaises(InvalidArguments):
            build_request(tool_by_name("set_scene"), {})

    def test_unknown_argument_rejected(self):
        with self.assertRaises(InvalidArguments):
            build_request(tool_by_name("set_animation"), {"animation": "wave", "speed": 5})


class BrightnessValidationTest(unittest.TestCase):
    def call(self, arguments):
        return build_request(tool_by_name("set_brightness"), arguments)

    def test_boundaries_accepted(self):
        for level in (0, 40, 100):
            method, path, payload = self.call({"level": level})
            self.assertEqual((method, path), ("POST", "/display/brightness/set"))
            self.assertEqual(payload, {"level": level})

    def test_out_of_bounds_rejected(self):
        for level in (-1, 101, 1000):
            with self.assertRaises(InvalidArguments):
                self.call({"level": level})

    def test_bool_rejected(self):
        with self.assertRaises(InvalidArguments):
            self.call({"level": True})

    def test_float_rejected(self):
        with self.assertRaises(InvalidArguments):
            self.call({"level": 50.0})

    def test_string_rejected(self):
        with self.assertRaises(InvalidArguments):
            self.call({"level": "50"})

    def test_missing_rejected(self):
        with self.assertRaises(InvalidArguments):
            self.call({})


class NoArgToolValidationTest(unittest.TestCase):
    def test_no_arg_tools_accept_empty_and_reject_anything(self):
        cases = {
            "clear_text": ("POST", "/display/text/clear"),
            "clear_display": ("POST", "/display/clear"),
            "get_state": ("GET", "/state"),
            "get_device_info": ("GET", "/identity"),
        }
        for name, expected in cases.items():
            method, path, payload = build_request(tool_by_name(name), {})
            self.assertEqual((method, path), expected)
            self.assertEqual(payload, {})
            method, path, _ = build_request(tool_by_name(name), None)
            self.assertEqual((method, path), expected)
            with self.assertRaises(InvalidArguments):
                build_request(tool_by_name(name), {"x": 1})


class ConditionalToolValidationTest(unittest.TestCase):
    def test_color_hue_boundaries(self):
        method, path, payload = build_request(tool_by_name("set_color"), {"hue": 0})
        self.assertEqual((method, path), ("POST", "/display/color/set"))
        self.assertEqual(payload, {"hue": 0})
        _, _, payload = build_request(tool_by_name("set_color"), {"hue": 359})
        self.assertEqual(payload, {"hue": 359})

    def test_color_hue_out_of_bounds_rejected(self):
        for hue in (-1, 360, 1000):
            with self.assertRaises(InvalidArguments):
                build_request(tool_by_name("set_color"), {"hue": hue})

    def test_color_scheme_accepted_and_capped(self):
        _, _, payload = build_request(tool_by_name("set_color"), {"scheme": "warm"})
        self.assertEqual(payload, {"scheme": "warm"})
        with self.assertRaises(InvalidArguments):
            build_request(tool_by_name("set_color"), {"scheme": "s" * 33})

    def test_color_requires_exactly_one_of_hue_or_scheme(self):
        with self.assertRaises(InvalidArguments):
            build_request(tool_by_name("set_color"), {})
        with self.assertRaises(InvalidArguments):
            build_request(tool_by_name("set_color"), {"hue": 10, "scheme": "warm"})

    def test_spectrum_start_defaults(self):
        method, path, payload = build_request(tool_by_name("start_spectrum"), {})
        self.assertEqual((method, path), ("POST", "/display/spectrum/start"))
        self.assertEqual(payload, {"source": "local_mic"})

    def test_spectrum_start_with_style(self):
        _, _, payload = build_request(
            tool_by_name("start_spectrum"), {"style": "spectrum.fire", "source": "local_mic"}
        )
        self.assertEqual(payload, {"style": "spectrum.fire", "source": "local_mic"})

    def test_spectrum_start_rejects_other_sources(self):
        with self.assertRaises(InvalidArguments):
            build_request(tool_by_name("start_spectrum"), {"source": "remote_pi"})

    def test_spectrum_start_rejects_bad_style(self):
        with self.assertRaises(InvalidArguments):
            build_request(tool_by_name("start_spectrum"), {"style": "bad\nstyle"})
        with self.assertRaises(InvalidArguments):
            build_request(tool_by_name("start_spectrum"), {"style": ""})

    def test_spectrum_stop(self):
        method, path, payload = build_request(tool_by_name("stop_spectrum"), {})
        self.assertEqual((method, path), ("POST", "/display/spectrum/stop"))
        self.assertEqual(payload, {})


def _default_args(spec):
    """Minimal valid arguments for each tool (for endpoint-mapping tests)."""
    return {
        "show_text": {"text": "X"},
        "clear_text": {},
        "set_scene": {"scene": "wave"},
        "set_animation": {"animation": "wave"},
        "set_expression": {"expression": "idle"},
        "set_brightness": {"level": 40},
        "clear_display": {},
        "get_state": {},
        "get_device_info": {},
        "set_color": {"hue": 10},
        "start_spectrum": {},
        "stop_spectrum": {},
    }[spec.name]


if __name__ == "__main__":
    unittest.main()
