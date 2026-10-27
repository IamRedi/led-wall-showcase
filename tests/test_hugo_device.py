"""HUGO DeviceAdapter device-client tests (C-L006-1/C-L006-8): typed
errors, bounded reads, fail-safe normalization, unreachable classification.

Wire shapes are independently transcribed from docs/api/CONTRACT.md; the
fake device is a real in-process HTTP server on 127.0.0.1 (loopback only).
"""

import http.server
import json
import socket
import sys
import threading
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from hugo_adapter.device import (  # noqa: E402
    DeviceClient,
    DeviceProtocolError,
    DeviceUnreachable,
    TypedDeviceError,
    normalize_identity,
    normalize_state,
)

CORE_CAPS = [
    "display.text.show", "display.text.clear", "display.scene.set",
    "display.animation.set", "display.expression.set", "display.clear",
    "display.brightness.set", "display.state.read",
]


class FakeDeviceHandler(http.server.BaseHTTPRequestHandler):
    """Configurable fake LED Wall; class attributes are set per test."""

    identity_body = {"ok": False}
    state_body = {"ok": False}
    responses = {}  # path -> (status, body-bytes)
    protocol_version = "HTTP/1.1"

    def log_message(self, fmt, *args):  # silence
        pass

    def _send(self, status, body: bytes):
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        if self.path == "/identity":
            self._send(200, json.dumps(FakeDeviceHandler.identity_body).encode())
        elif self.path == "/state":
            self._send(200, json.dumps(FakeDeviceHandler.state_body).encode())
        else:
            self._send(404, b'{"ok":false,"error":{"code":"unsupportedCapability","message":"unknown path"}}')

    def do_POST(self):
        length = int(self.headers.get("Content-Length", "0"))
        self.rfile.read(length)
        status, body = FakeDeviceHandler.responses.get(
            self.path, (200, b'{"ok":true}')
        )
        self._send(status, body)


class DeviceServer:
    def __init__(self):
        self.server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), FakeDeviceHandler)
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)
        self.thread.start()
        self.port = self.server.server_address[1]

    @property
    def url(self):
        return f"http://127.0.0.1:{self.port}"

    def stop(self):
        self.server.shutdown()
        self.server.server_close()
        self.thread.join(timeout=5)


class NormalizeStateTest(unittest.TestCase):
    def test_full_state_normalized(self):
        raw = {
            "available": True,
            "deviceId": "led-wall-abc123",
            "baseMode": "text",
            "currentMode": "scene",
            "activeScene": None,
            "activeAnimation": None,
            "activeExpression": None,
            "activeTransient": {"kind": "scene", "remainingMs": 4321},
            "text": {"current": "HELLO", "scrolling": True},
            "brightness": {"current": 40, "max": 60},
            "uptimeSeconds": 98765,
            "warnings": ["cfg: unknown key x"],
        }
        state = normalize_state(raw)
        self.assertTrue(state.available)
        self.assertEqual(state.device_id, "led-wall-abc123")
        self.assertEqual(state.base_mode, "text")
        self.assertEqual(state.current_mode, "scene")
        self.assertEqual(state.transient_kind, "scene")
        self.assertEqual(state.transient_remaining_ms, 4321)
        self.assertEqual(state.text_current, "HELLO")
        self.assertTrue(state.text_scrolling)
        self.assertEqual(state.brightness_current, 40)
        self.assertEqual(state.brightness_max, 60)
        self.assertEqual(state.uptime_seconds, 98765)
        self.assertEqual(state.warnings, ("cfg: unknown key x",))

    def test_non_dict_falls_back_to_neutral_defaults(self):
        state = normalize_state("junk")
        self.assertTrue(state.available)
        self.assertEqual(state.base_mode, "idle")
        self.assertEqual(state.current_mode, "idle")
        self.assertIsNone(state.transient_kind)
        self.assertEqual(state.brightness_current, 0)
        self.assertEqual(state.warnings, ())

    def test_wrong_types_fail_safe(self):
        state = normalize_state(
            {
                "available": "yes",
                "deviceId": 7,
                "baseMode": ["text"],
                "activeTransient": "scene",
                "text": "HELLO",
                "brightness": [1, 2],
                "uptimeSeconds": True,
                "warnings": {"a": 1},
            }
        )
        self.assertTrue(state.available)  # non-bool -> default True
        self.assertEqual(state.device_id, "led-wall")
        self.assertEqual(state.base_mode, "idle")
        self.assertIsNone(state.transient_kind)
        self.assertIsNone(state.text_current)
        self.assertEqual(state.brightness_current, 0)
        self.assertEqual(state.uptime_seconds, 0)
        self.assertEqual(state.warnings, ())

    def test_text_capped_at_128(self):
        state = normalize_state({"text": {"current": "A" * 200, "scrolling": False}})
        self.assertEqual(len(state.text_current), 128)

    def test_warnings_capped_16x200(self):
        state = normalize_state({"warnings": ["w" * 500] * 30})
        self.assertEqual(len(state.warnings), 16)
        self.assertTrue(all(len(w) == 200 for w in state.warnings))

    def test_negative_transient_remaining_clamped(self):
        state = normalize_state({"activeTransient": {"kind": "text", "remainingMs": -5}})
        self.assertEqual(state.transient_remaining_ms, 0)

    def test_transient_rejects_bool_remaining_and_non_int(self):
        for bad in (True, "9", 1.5):
            state = normalize_state({"activeTransient": {"kind": "text", "remainingMs": bad}})
            self.assertIsNone(state.transient_kind)

    def test_bool_brightness_rejected_as_int(self):
        state = normalize_state({"brightness": {"current": True, "max": 60}})
        self.assertEqual(state.brightness_current, 0)


class NormalizeIdentityTest(unittest.TestCase):
    def test_full_identity_normalized_including_public_identity_fields(self):
        identity = normalize_identity(
            {
                "deviceId": "led-wall-efuse24",
                "deviceType": "visual_display_v1",
                "role": "display",
                "protocolVersion": 1,
                "fwVersion": "1.3.0",
                "capabilities": CORE_CAPS + ["display.spectrum.start", "display.spectrum.stop"],
                "matrix": {"width": 30, "height": 10},
            }
        )
        self.assertEqual(identity.device_id, "led-wall-efuse24")
        self.assertEqual(identity.device_type, "visual_display_v1")
        self.assertEqual(identity.role, "display")
        self.assertEqual(identity.protocol_version, 1)
        self.assertEqual(identity.fw_version, "1.3.0")
        self.assertEqual(
            identity.capabilities,
            tuple(CORE_CAPS + ["display.spectrum.start", "display.spectrum.stop"]),
        )
        self.assertEqual((identity.matrix_width, identity.matrix_height), (30, 10))

    def test_defaults_fail_safe(self):
        identity = normalize_identity({})
        self.assertEqual(identity.device_id, "led-wall")
        self.assertEqual(identity.device_type, "visual_display_v1")
        self.assertEqual(identity.role, "display")
        self.assertEqual(identity.protocol_version, 1)
        self.assertEqual(identity.fw_version, "unknown")
        self.assertEqual(identity.capabilities, ())
        self.assertEqual((identity.matrix_width, identity.matrix_height), (0, 0))

    def test_capability_list_capped_and_deduped(self):
        # Cap semantics inherited from the accepted LED-L005 client (COPY-ADAPT):
        # the 16-entry cap applies to raw entries before dedupe — duplicates
        # consume cap slots. Core capability sets (8 entries) are unaffected.
        identity = normalize_identity({"capabilities": ["a", "a", "b"] + [f"c{i}" for i in range(20)]})
        self.assertEqual(identity.capabilities, tuple(["a", "b"] + [f"c{i}" for i in range(13)]))
        identity = normalize_identity({"capabilities": ["x", "x", "y"]})
        self.assertEqual(identity.capabilities, ("x", "y"))

    def test_matrix_clamped(self):
        identity = normalize_identity({"matrix": {"width": 5000, "height": -3}})
        self.assertEqual((identity.matrix_width, identity.matrix_height), (1024, 0))


class ClientConstructionTest(unittest.TestCase):
    def test_non_http_url_rejected(self):
        with self.assertRaises(ValueError):
            DeviceClient("https://device.local")

    def test_userinfo_url_rejected(self):
        with self.assertRaises(ValueError):
            DeviceClient("http://user:pass@127.0.0.1:9")
        with self.assertRaises(ValueError):
            DeviceClient("http://user@127.0.0.1:9")

    def test_userinfo_in_path_accepted(self):
        client = DeviceClient("http://127.0.0.1:9/x@y")
        self.assertEqual(client._base, "http://127.0.0.1:9/x@y")

    def test_trailing_slash_normalized(self):
        client = DeviceClient("http://127.0.0.1:9/")
        self.assertEqual(client._base, "http://127.0.0.1:9")


class ClientTransportTest(unittest.TestCase):
    def setUp(self):
        FakeDeviceHandler.identity_body = {
            "deviceId": "led-wall-t1",
            "deviceType": "visual_display_v1",
            "role": "display",
            "protocolVersion": 1,
            "fwVersion": "1.3.0",
            "capabilities": CORE_CAPS,
            "matrix": {"width": 30, "height": 10},
        }
        FakeDeviceHandler.state_body = {
            "available": True,
            "deviceId": "led-wall-t1",
            "baseMode": "idle",
            "currentMode": "idle",
            "brightness": {"current": 40, "max": 60},
            "uptimeSeconds": 10,
            "warnings": [],
        }
        FakeDeviceHandler.responses = {}
        self.server = DeviceServer()
        self.client = DeviceClient(self.server.url, timeout_s=2.0)

    def tearDown(self):
        self.server.stop()

    def test_identity_round_trip(self):
        identity = self.client.identity()
        self.assertEqual(identity.device_id, "led-wall-t1")
        self.assertEqual(identity.capabilities, tuple(CORE_CAPS))

    def test_state_round_trip(self):
        state = self.client.state()
        self.assertEqual(state.base_mode, "idle")
        self.assertEqual(state.brightness_current, 40)

    def test_command_round_trip(self):
        self.client.command("/display/text/show", {"text": "HEY"})
        self.assertIsNone(None)  # no exception == accepted

    def test_typed_error_envelope_400(self):
        FakeDeviceHandler.responses = {
            "/display/scene/set": (
                400,
                b'{"ok":false,"error":{"code":"unknownScene","message":"unknown scene","available":"corners, wave"}}',
            )
        }
        with self.assertRaises(TypedDeviceError) as ctx:
            self.client.command("/display/scene/set", {"scene": "nope"})
        self.assertEqual(ctx.exception.code, "unknownScene")
        self.assertEqual(ctx.exception.available, "corners, wave")

    def test_typed_error_envelope_404(self):
        FakeDeviceHandler.responses = {
            "/display/nope": (
                404,
                b'{"ok":false,"error":{"code":"unsupportedCapability","message":"unknown path"}}',
            )
        }
        with self.assertRaises(TypedDeviceError) as ctx:
            self.client.command("/display/nope", {})
        self.assertEqual(ctx.exception.code, "unsupportedCapability")

    def test_plain_error_without_envelope_is_protocol_error(self):
        FakeDeviceHandler.responses = {"/display/clear": (500, b"Internal Server Error")}
        with self.assertRaises(DeviceProtocolError):
            self.client.command("/display/clear", {})

    def test_non_json_200_body_is_protocol_error(self):
        FakeDeviceHandler.responses = {"/display/clear": (200, b"<html>not json</html>")}
        with self.assertRaises(DeviceProtocolError):
            self.client.command("/display/clear", {})

    def test_ok_false_200_envelope_classified(self):
        FakeDeviceHandler.responses = {
            "/display/clear": (200, b'{"ok":false,"error":{"code":"internalError","message":"x"}}')
        }
        with self.assertRaises(TypedDeviceError) as ctx:
            self.client.command("/display/clear", {})
        self.assertEqual(ctx.exception.code, "internalError")

    def test_oversized_body_is_protocol_error(self):
        FakeDeviceHandler.state_body = {"blob": "x" * (64 * 1024 + 10)}
        with self.assertRaises(DeviceProtocolError):
            self.client.state()

    def test_unknown_path_get_is_typed_error(self):
        with self.assertRaises(TypedDeviceError):
            self.client._request("/nope", None)

    def test_connection_refused_is_unreachable(self):
        dead = DeviceServer()
        url = dead.url
        dead.stop()
        client = DeviceClient(url, timeout_s=1.0)
        with self.assertRaises(DeviceUnreachable):
            client.state()

    def test_timeout_is_unreachable(self):
        listener = socket.socket()
        listener.bind(("127.0.0.1", 0))
        listener.listen(1)
        port = listener.getsockname()[1]
        try:
            client = DeviceClient(f"http://127.0.0.1:{port}", timeout_s=0.5)
            with self.assertRaises(DeviceUnreachable):
                client.state()
        finally:
            listener.close()


if __name__ == "__main__":
    unittest.main()
