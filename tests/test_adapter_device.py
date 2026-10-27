"""LED Wall API client tests (C-L005-1): typed errors, bounds, fail-safe
normalization — expectations derived from docs/api/CONTRACT.md and the frozen
§4/§9 semantics, not from the implementation."""

import http.server
import json
import socket
import sys
import threading
import time
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from ha_adapter.device import (  # noqa: E402
    DeviceClient,
    DeviceProtocolError,
    DeviceState,
    DeviceUnreachable,
    TypedDeviceError,
    normalize_identity,
    normalize_state,
)

FULL_STATE = {
    "available": True,
    "deviceId": "led-wall-ab12cd34",
    "deviceType": "visual_display_v1",
    "protocolVersion": 1,
    "fwVersion": "0.1.0-test",
    "baseMode": "scene",
    "currentMode": "animation",
    "activeTransient": {"kind": "animation", "remainingMs": 4000},
    "activeScene": "wave",
    "activeAnimation": None,
    "activeExpression": None,
    "text": None,
    "brightness": {"current": 40, "max": 80},
    "spectrum": {"available": False, "active": False},
    "matrix": {"width": 30, "height": 10},
    "clients": {"count": 1},
    "warnings": ["ignored unknown config key: wifi.extra"],
    "uptimeSeconds": 90,
}


class _Handler(http.server.BaseHTTPRequestHandler):
    behavior = {}  # class-level: path -> (status, body_bytes)
    sleep_s = 0.0
    body_cap_probe = False

    def log_message(self, *args):  # silence test noise
        pass

    def _respond(self):
        if _Handler.sleep_s:
            time.sleep(_Handler.sleep_s)
        target = self.path.split("?")[0]
        entry = _Handler.behavior.get(target)
        if entry is None:
            self.send_response(404)
            self.send_header("Content-Type", "application/json")
            self.end_headers()
            self.wfile.write(b'{"ok":false,"error":{"code":"unsupportedCapability","message":"unknown path"}}')
            return
        status, body = entry
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        if status in (301, 302, 303, 307, 308):
            self.send_header("Location", "/elsewhere")
        self.end_headers()
        if _Handler.body_cap_probe:
            self.wfile.write(body + b"x" * (128 * 1024))
        else:
            self.wfile.write(body)

    def do_GET(self):
        self._respond()

    def do_POST(self):
        length = int(self.headers.get("Content-Length", "0"))
        self.rfile.read(length)
        self._respond()


class DeviceClientTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), _Handler)
        cls.port = cls.server.server_address[1]
        cls.thread = threading.Thread(target=cls.server.serve_forever, daemon=True)
        cls.thread.start()

    @classmethod
    def tearDownClass(cls):
        cls.server.shutdown()
        cls.server.server_close()

    def setUp(self):
        _Handler.behavior = {}
        _Handler.sleep_s = 0.0
        _Handler.body_cap_probe = False
        self.client = DeviceClient(f"http://127.0.0.1:{self.port}", timeout_s=2.0)

    def test_state_success_normalizes_full_payload(self):
        _Handler.behavior["/state"] = (200, json.dumps(FULL_STATE).encode())
        state = self.client.state()
        self.assertEqual(state.device_id, "led-wall-ab12cd34")
        self.assertEqual(state.base_mode, "scene")
        self.assertEqual(state.current_mode, "animation")
        self.assertEqual(state.active_scene, "wave")
        self.assertEqual((state.transient_kind, state.transient_remaining_ms), ("animation", 4000))
        self.assertEqual((state.brightness_current, state.brightness_max), (40, 80))
        self.assertEqual(state.text_current, None)
        self.assertEqual(state.uptime_seconds, 90)
        self.assertEqual(state.warnings, ("ignored unknown config key: wifi.extra",))

    def test_identity_success(self):
        identity_payload = {
            "deviceId": "led-wall-ab12cd34",
            "deviceType": "visual_display_v1",
            "role": "display",
            "protocolVersion": 1,
            "fwVersion": "0.1.0-test",
            "capabilities": [
                "display.text.show", "display.text.clear", "display.scene.set",
                "display.animation.set", "display.expression.set", "display.clear",
                "display.brightness.set", "display.state.read",
            ],
            "matrix": {"width": 30, "height": 10},
        }
        _Handler.behavior["/identity"] = (200, json.dumps(identity_payload).encode())
        identity = self.client.identity()
        self.assertEqual(identity.device_id, "led-wall-ab12cd34")
        self.assertEqual(len(identity.capabilities), 8)

    def test_command_typed_error_envelope(self):
        _Handler.behavior["/display/scene/set"] = (
            400,
            json.dumps(
                {"ok": False, "error": {"code": "unknownScene", "message": "unknown scene",
                                        "available": "a, b"}}
            ).encode(),
        )
        with self.assertRaises(TypedDeviceError) as ctx:
            self.client.command("/display/scene/set", {"scene": "nope"})
        self.assertEqual(ctx.exception.code, "unknownScene")
        self.assertEqual(ctx.exception.message, "unknown scene")
        self.assertEqual(ctx.exception.available, "a, b")

    def test_command_404_with_typed_envelope(self):
        _Handler.behavior["/display/scene/set"] = (404, json.dumps(
            {"ok": False, "error": {"code": "unsupportedCapability", "message": "unknown path"}}
        ).encode())
        with self.assertRaises(TypedDeviceError) as ctx:
            self.client.command("/display/scene/set", {"scene": "x"})
        self.assertEqual(ctx.exception.code, "unsupportedCapability")

    def test_command_400_without_envelope_is_protocol_error(self):
        _Handler.behavior["/display/scene/set"] = (400, b"not json")
        with self.assertRaises(DeviceProtocolError):
            self.client.command("/display/scene/set", {})

    def test_server_500_plain_text_is_protocol_error(self):
        _Handler.behavior["/state"] = (500, b"boom")
        with self.assertRaises(DeviceProtocolError):
            self.client.state()

    def test_malformed_json_200_is_protocol_error(self):
        _Handler.behavior["/state"] = (200, b"{broken")
        with self.assertRaises(DeviceProtocolError):
            self.client.state()

    def test_oversized_body_is_protocol_error(self):
        _Handler.behavior["/state"] = (200, b"{")
        _Handler.body_cap_probe = True
        with self.assertRaises(DeviceProtocolError):
            self.client.state()

    def test_redirect_is_not_followed(self):
        _Handler.behavior["/state"] = (302, b"")
        _Handler.behavior["/elsewhere"] = (200, json.dumps(FULL_STATE).encode())
        with self.assertRaises(DeviceProtocolError):
            self.client.state()

    def test_connection_refused_is_unreachable(self):
        # Bind then close to get a deterministic refused port.
        sock = socket.socket()
        sock.bind(("127.0.0.1", 0))
        port = sock.getsockname()[1]
        sock.close()
        client = DeviceClient(f"http://127.0.0.1:{port}", timeout_s=0.5)
        with self.assertRaises(DeviceUnreachable):
            client.state()

    def test_timeout_is_unreachable(self):
        _Handler.behavior["/state"] = (200, json.dumps(FULL_STATE).encode())
        _Handler.sleep_s = 1.0
        client = DeviceClient(f"http://127.0.0.1:{self.port}", timeout_s=0.2)
        with self.assertRaises(DeviceUnreachable):
            client.state()

    def test_non_http_url_rejected(self):
        with self.assertRaises(ValueError):
            DeviceClient("https://led-wall", timeout_s=1.0)


class NormalizationFailSafeTest(unittest.TestCase):
    def test_missing_fields_yield_safe_defaults(self):
        state = normalize_state({})
        self.assertIsInstance(state, DeviceState)
        self.assertTrue(state.available)
        self.assertEqual(state.device_id, "led-wall")
        self.assertEqual(state.base_mode, "idle")
        self.assertEqual(state.current_mode, "idle")
        self.assertIsNone(state.active_scene)
        self.assertIsNone(state.transient_kind)
        self.assertIsNone(state.text_current)
        self.assertEqual((state.brightness_current, state.brightness_max), (0, 0))
        self.assertEqual(state.warnings, ())

    def test_non_object_payload_yields_defaults(self):
        state = normalize_state([1, 2, 3])
        self.assertEqual(state.base_mode, "idle")

    def test_wrongly_typed_fields_fall_back(self):
        raw = dict(FULL_STATE)
        raw["baseMode"] = 42
        raw["brightness"] = {"current": "high", "max": None}
        raw["activeScene"] = 7
        raw["activeTransient"] = {"kind": 5, "remainingMs": "soon"}
        raw["text"] = "not-an-object"
        raw["uptimeSeconds"] = -5
        state = normalize_state(raw)
        self.assertEqual(state.base_mode, "idle")
        self.assertEqual((state.brightness_current, state.brightness_max), (0, 0))
        self.assertIsNone(state.active_scene)
        self.assertIsNone(state.transient_kind)
        self.assertIsNone(state.text_current)
        self.assertEqual(state.uptime_seconds, 0)

    def test_booleans_are_not_accepted_as_integers(self):
        state = normalize_state({"brightness": {"current": True, "max": False}})
        self.assertEqual((state.brightness_current, state.brightness_max), (0, 0))

    def test_text_is_capped_at_protocol_limit(self):
        state = normalize_state({"text": {"current": "A" * 5000, "scrolling": "yes"}})
        self.assertEqual(len(state.text_current), 128)
        self.assertFalse(state.text_scrolling)

    def test_warnings_are_bounded(self):
        state = normalize_state({"warnings": [f"w{i}" for i in range(50)] + [7, None]})
        self.assertEqual(len(state.warnings), 16)
        self.assertEqual(state.warnings[0], "w0")

    def test_transient_remaining_is_clamped_non_negative(self):
        state = normalize_state({"activeTransient": {"kind": "text", "remainingMs": -3}})
        self.assertEqual(state.transient_remaining_ms, 0)

    def test_identity_fails_safe(self):
        identity = normalize_identity({"capabilities": ["display.clear", "display.clear", 5, None]})
        self.assertEqual(identity.device_id, "led-wall")
        self.assertEqual(identity.capabilities, ("display.clear",))
        self.assertEqual(identity.fw_version, "unknown")


if __name__ == "__main__":
    unittest.main()
