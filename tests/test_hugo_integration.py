"""Deterministic in-process integration round-trip for the HUGO DeviceAdapter
(C-L006-1/C-L006-7/C-L006-8, V2).

A fake LED Wall device (HTTP, exactly the docs/api/CONTRACT.md surface) runs
on a 127.0.0.1 ephemeral port; the real DeviceClient, capability cache, intent
catalog, JSON-RPC dispatch, and stdio loop run against it. This is the
host-verifiable substitute for live V3/V4 checks (real HUGO host + real
device), which remain pending infrastructure/device authorization.
"""

import http.server
import io
import json
import sys
import threading
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from hugo_adapter.config import AdapterConfig  # noqa: E402
from hugo_adapter.device import DeviceClient  # noqa: E402
from hugo_adapter.service import DeviceAdapterService  # noqa: E402

CORE_CAPS = [
    "display.text.show", "display.text.clear", "display.scene.set",
    "display.animation.set", "display.expression.set", "display.clear",
    "display.brightness.set", "display.state.read",
]
CORE_TOOL_NAMES = {
    "show_text", "clear_text", "set_scene", "set_animation", "set_expression",
    "set_brightness", "clear_display", "get_state", "get_device_info",
}


class FakeClock:
    def __init__(self):
        self.now = 1000.0

    def __call__(self):
        return self.now

    def advance(self, seconds):
        self.now += seconds


class FakeDevice:
    """Configurable fake LED Wall speaking the CONTRACT.md surface."""

    def __init__(self, advertise=(), port=0):
        self.requests = []  # (method, path, body)
        self.identity_requests = 0
        self.lock = threading.Lock()
        self.advertise = list(advertise)
        self.identity_ok = True
        self.state_payload = {
            "available": True,
            "deviceId": "led-wall-it1",
            "deviceType": "visual_display_v1",
            "protocolVersion": 1,
            "fwVersion": "1.4.0",
            "baseMode": "idle",
            "currentMode": "idle",
            "activeScene": None,
            "activeAnimation": None,
            "activeExpression": None,
            "activeTransient": None,
            "brightness": {"current": 40, "max": 60},
            "spectrum": {"available": False, "active": False},
            "matrix": {"width": 30, "height": 10},
            "clients": {"count": 1},
            "warnings": [],
            "uptimeSeconds": 42,
        }
        self.error_for = {}  # path -> (code, message, available)
        outer = self

        class Handler(http.server.BaseHTTPRequestHandler):
            protocol_version = "HTTP/1.1"

            def log_message(self, fmt, *args):
                pass

            def _send(self, status, body):
                data = json.dumps(body).encode()
                self.send_response(status)
                self.send_header("Content-Type", "application/json")
                self.send_header("Content-Length", str(len(data)))
                self.end_headers()
                self.wfile.write(data)

            def do_GET(self):
                with outer.lock:
                    outer.requests.append(("GET", self.path, None))
                    if self.path == "/identity":
                        outer.identity_requests += 1
                        if not outer.identity_ok:
                            self._send(500, {"boom": True})
                            return
                        self._send(
                            200,
                            {
                                "deviceId": "led-wall-it1",
                                "deviceType": "visual_display_v1",
                                "role": "display",
                                "protocolVersion": 1,
                                "fwVersion": "1.4.0",
                                "capabilities": CORE_CAPS + outer.advertise,
                                "matrix": {"width": 30, "height": 10},
                            },
                        )
                    elif self.path == "/state":
                        self._send(200, dict(outer.state_payload))
                    else:
                        self._send(
                            404,
                            {"ok": False, "error": {"code": "unsupportedCapability", "message": "unknown path"}},
                        )

            def do_POST(self):
                length = int(self.headers.get("Content-Length", "0"))
                body = json.loads(self.rfile.read(length) or b"{}")
                with outer.lock:
                    outer.requests.append(("POST", self.path, body))
                    error = outer.error_for.get(self.path)
                if error is not None:
                    code, message, available = error
                    envelope = {"ok": False, "error": {"code": code, "message": message}}
                    if available is not None:
                        envelope["error"]["available"] = available
                    self._send(400, envelope)
                else:
                    self._send(200, {"ok": True})

        self.server = http.server.ThreadingHTTPServer(("127.0.0.1", port), Handler)
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)
        self.thread.start()
        self.port = self.server.server_address[1]

    @property
    def url(self):
        return f"http://127.0.0.1:{self.port}"

    def command_requests(self):
        return [r for r in self.requests if r[0] == "POST"]

    def stop(self):
        self.server.shutdown()
        self.server.server_close()
        self.thread.join(timeout=5)


class ServiceTestBase(unittest.TestCase):
    def make_service(self, device, ttl=5):
        self.clock = FakeClock()
        self.logs = []
        config = AdapterConfig(
            device_url=device.url, device_timeout_s=2.0, capability_ttl_s=ttl, log_level="INFO"
        )
        client = DeviceClient(device.url, timeout_s=2.0)
        return DeviceAdapterService(client, config, clock=self.clock, log=self.logs.append)

    @staticmethod
    def line(method, params=None, request_id=1):
        request = {"jsonrpc": "2.0", "method": method, "id": request_id}
        if params is not None:
            request["params"] = params
        return json.dumps(request)

    @staticmethod
    def call(service, name, arguments=None, request_id=1):
        params = {"name": name}
        if arguments is not None:
            params["arguments"] = arguments
        response = service.handle_line(ServiceTestBase.line("tools/call", params, request_id))
        assert response is not None
        return json.loads(response)

    @staticmethod
    def listing(service):
        response = service.handle_line(ServiceTestBase.line("tools/list", {}))
        assert response is not None
        return json.loads(response)["result"]["tools"]


class ToolsListTest(ServiceTestBase):
    def test_core_only_identity_lists_nine_core_tools(self):
        device = FakeDevice()
        try:
            service = self.make_service(device)
            tools = self.listing(service)
            self.assertEqual({t["name"] for t in tools}, CORE_TOOL_NAMES)
            for tool in tools:
                self.assertNotIn("requiresCapability", tool)
        finally:
            device.stop()

    def test_advertised_capabilities_add_conditional_tools(self):
        device = FakeDevice(advertise=["display.color.set", "display.spectrum.start", "display.spectrum.stop"])
        try:
            service = self.make_service(device)
            tools = {t["name"]: t for t in self.listing(service)}
            self.assertEqual(set(tools), CORE_TOOL_NAMES | {"set_color", "start_spectrum", "stop_spectrum"})
            self.assertEqual(tools["set_color"]["requiresCapability"], "display.color.set")
            self.assertEqual(tools["start_spectrum"]["requiresCapability"], "display.spectrum.start")
        finally:
            device.stop()

    def test_capability_cache_refreshes_after_ttl(self):
        device = FakeDevice()
        try:
            service = self.make_service(device, ttl=5)
            self.listing(service)
            first = device.identity_requests
            self.listing(service)  # within TTL: no new fetch
            self.assertEqual(device.identity_requests, first)
            self.clock.advance(6)
            self.listing(service)  # past TTL: one new fetch
            self.assertEqual(device.identity_requests, first + 1)
        finally:
            device.stop()


class ToolCallRoundTripTest(ServiceTestBase):
    def setUp(self):
        self.device = FakeDevice()
        self.service = self.make_service(self.device)

    def tearDown(self):
        self.device.stop()

    def test_show_text_round_trip_exact_wire_request(self):
        response = self.call(self.service, "show_text", {"text": "HELLO", "scroll": True, "speedMsPerPx": 90})
        self.assertEqual(response["result"], {"ok": True, "tool": "show_text", "executed": True})
        self.assertEqual(
            self.device.requests[-1],
            ("POST", "/display/text/show", {"text": "HELLO", "scroll": True, "speedMsPerPx": 90}),
        )

    def test_get_state_round_trip_content(self):
        self.device.state_payload.update(
            {
                "baseMode": "expression",
                "currentMode": "expression",
                "activeExpression": "alert",
                "brightness": {"current": 55, "max": 60},
            }
        )
        response = self.call(self.service, "get_state")
        state = response["result"]["state"]
        self.assertEqual(state["baseMode"], "expression")
        self.assertEqual(state["activeExpression"], "alert")
        self.assertEqual(state["brightness"], {"current": 55, "max": 60})
        self.assertEqual(state["uptimeSeconds"], 42)
        self.assertIsNone(state["activeTransient"])
        self.assertEqual(self.device.requests[-1], ("GET", "/state", None))

    def test_get_device_info_round_trip_content(self):
        response = self.call(self.service, "get_device_info")
        identity = response["result"]["identity"]
        self.assertEqual(identity["deviceId"], "led-wall-it1")
        self.assertEqual(identity["deviceType"], "visual_display_v1")
        self.assertEqual(identity["role"], "display")
        self.assertEqual(identity["protocolVersion"], 1)
        self.assertEqual(identity["matrix"], {"width": 30, "height": 10})
        self.assertEqual(identity["capabilities"], CORE_CAPS)

    def test_transient_text_payload_forwarded(self):
        self.call(self.service, "show_text", {"text": "BRB", "durationMs": 2500})
        self.assertEqual(
            self.device.requests[-1],
            ("POST", "/display/text/show", {"text": "BRB", "durationMs": 2500}),
        )

    def test_typed_device_error_surfaced_with_available_list(self):
        self.device.error_for["/display/expression/set"] = (
            "unknownExpression",
            "unknown expression",
            "idle, listening, thinking, speaking, happy, alert, error, sleeping",
        )
        response = self.call(self.service, "set_expression", {"expression": "confused"})
        error = response["error"]
        self.assertEqual(error["code"], -32000)
        self.assertEqual(error["data"]["kind"], "deviceError")
        self.assertEqual(error["data"]["code"], "unknownExpression")
        self.assertEqual(
            error["data"]["available"],
            "idle, listening, thinking, speaking, happy, alert, error, sleeping",
        )

    def test_id_correlation_string_and_int(self):
        response = json.loads(
            self.service.handle_line(self.line("tools/call", {"name": "clear_display"}, "req-42"))
        )
        self.assertEqual(response["id"], "req-42")
        response = json.loads(
            self.service.handle_line(self.line("tools/call", {"name": "clear_display"}, 99))
        )
        self.assertEqual(response["id"], 99)


class RejectionTest(ServiceTestBase):
    def setUp(self):
        self.device = FakeDevice()
        self.service = self.make_service(self.device)

    def tearDown(self):
        self.device.stop()

    def test_invalid_arguments_never_reach_device(self):
        before = len(self.device.requests)
        response = self.call(self.service, "show_text", {"text": "A" * 129})
        self.assertEqual(response["error"]["code"], -32602)
        self.assertEqual(response["error"]["data"]["kind"], "invalidArguments")
        self.assertEqual(len(self.device.requests), before)

    def test_unknown_tool_rejected_without_device_contact(self):
        before = len(self.device.requests)
        response = self.call(self.service, "draw_raw_leds", {"index": 5})
        self.assertEqual(response["error"]["data"]["kind"], "unknownTool")
        self.assertEqual(len(self.device.requests), before)

    def test_conditional_tool_without_advertisement_fails_closed(self):
        before_commands = len(self.device.command_requests())
        response = self.call(self.service, "set_color", {"hue": 10})
        self.assertEqual(response["error"]["code"], -32000)
        self.assertEqual(response["error"]["data"]["kind"], "unsupportedCapability")
        self.assertEqual(response["error"]["data"]["capability"], "display.color.set")
        self.assertEqual(len(self.device.command_requests()), before_commands)

    def test_array_params_rejected(self):
        response = json.loads(
            self.service.handle_line(self.line("tools/call", [{"name": "ping"}]))
        )
        self.assertEqual(response["error"]["code"], -32602)

    def test_unknown_parameter_in_call_rejected(self):
        before = len(self.device.requests)
        response = json.loads(
            self.service.handle_line(self.line("tools/call", {"name": "ping", "extra": 1}))
        )
        self.assertEqual(response["error"]["code"], -32602)
        self.assertEqual(len(self.device.requests), before)

    def test_unknown_method_rejected(self):
        response = json.loads(self.service.handle_line(self.line("system/reboot")))
        self.assertEqual(response["error"]["code"], -32601)
        self.assertEqual(response["error"]["data"]["kind"], "unknownMethod")


class CapabilityFailClosedTest(ServiceTestBase):
    def test_refresh_failure_hides_conditional_tools_and_fails_closed(self):
        device = FakeDevice(advertise=["display.color.set"])
        try:
            service = self.make_service(device, ttl=5)
            self.assertIn("set_color", {t["name"] for t in self.listing(service)})
            self.call(service, "set_color", {"hue": 10})  # works while advertised
            device.identity_ok = False  # device serves 500 on /identity
            self.clock.advance(6)  # past TTL
            self.assertNotIn("set_color", {t["name"] for t in self.listing(service)})
            response = self.call(service, "set_color", {"hue": 10})
            self.assertEqual(response["error"]["data"]["kind"], "unsupportedCapability")
            device.identity_ok = True
            self.clock.advance(6)
            self.assertIn("set_color", {t["name"] for t in self.listing(service)})
        finally:
            device.stop()

    def test_capability_refresh_is_rate_limited_against_dead_device(self):
        # Adversarial: a dead device must not turn every tools/list into an
        # identity attempt (retry-storm guard: at most one attempt per TTL).
        device = FakeDevice()
        try:
            service = self.make_service(device, ttl=5)
            self.listing(service)
            healthy = device.identity_requests
            device.identity_ok = False
            self.clock.advance(6)
            self.listing(service)  # first past-TTL listing: one attempt
            self.assertEqual(device.identity_requests, healthy + 1)
            self.listing(service)  # immediately again: rate-limited, no attempt
            self.listing(service)
            self.assertEqual(device.identity_requests, healthy + 1)
            self.clock.advance(6)
            self.listing(service)  # next TTL window: one more attempt
            self.assertEqual(device.identity_requests, healthy + 2)
        finally:
            device.stop()

    def test_capability_loss_after_re_advertisement_change(self):
        # Simulate a device firmware change that drops the color capability.
        device = FakeDevice(advertise=["display.color.set"])
        try:
            service = self.make_service(device, ttl=5)
            self.call(service, "set_color", {"hue": 10})
            device.advertise = []
            self.clock.advance(6)
            response = self.call(service, "set_color", {"hue": 10})
            self.assertEqual(response["error"]["data"]["kind"], "unsupportedCapability")
        finally:
            device.stop()


class OutageRecoveryTest(ServiceTestBase):
    def test_device_outage_surfaces_unreachable_and_service_keeps_running(self):
        device = FakeDevice()
        port = device.port
        service = self.make_service(device, ttl=5)
        self.call(service, "show_text", {"text": "ONLINE"})
        device.stop()
        response = self.call(service, "show_text", {"text": "OFFLINE"})
        self.assertEqual(response["error"]["code"], -32000)
        self.assertEqual(response["error"]["data"]["kind"], "deviceUnreachable")
        # The service must keep answering (queries degrade, core tools still listed).
        listing = json.loads(service.handle_line(self.line("tools/list", {})))
        self.assertEqual({t["name"] for t in listing["result"]["tools"]}, CORE_TOOL_NAMES)
        # Recovery: a device comes back on the same port.
        revived = FakeDevice(port=port)
        try:
            self.clock.advance(6)
            response = self.call(service, "show_text", {"text": "BACK"})
            self.assertEqual(response["result"]["executed"], True)
        finally:
            revived.stop()


class StdioLoopTest(ServiceTestBase):
    def test_run_end_to_end_protocol_lines(self):
        device = FakeDevice()
        try:
            service = self.make_service(device)
            requests = b"".join(
                [
                    b"\n",  # blank line: skipped, never answered
                    b"{not json}\n",  # parse error -> -32700 with id null
                    self.line("no/such/method", None, 7).encode() + b"\n",
                    self.line("ping", {}, "p-1").encode() + b"\n",
                    self.line("tools/call", {"name": "show_text", "arguments": {"text": "LOOP"}}, 8).encode() + b"\n",
                ]
            )
            stdin = io.BytesIO(requests)
            stdout = io.BytesIO()
            service.run(stdin=stdin, stdout=stdout)
            lines = stdout.getvalue().decode().strip().split("\n")
            self.assertEqual(len(lines), 4)  # blank line produced no response
            parse_error = json.loads(lines[0])
            self.assertEqual(parse_error["id"], None)
            self.assertEqual(parse_error["error"]["code"], -32700)
            method_error = json.loads(lines[1])
            self.assertEqual(method_error["id"], 7)
            self.assertEqual(method_error["error"]["code"], -32601)
            pong = json.loads(lines[2])
            self.assertEqual(pong["id"], "p-1")
            self.assertEqual(pong["result"]["pong"], True)
            show = json.loads(lines[3])
            self.assertEqual(show["id"], 8)
            self.assertEqual(show["result"]["tool"], "show_text")
            self.assertEqual(device.requests[-1], ("POST", "/display/text/show", {"text": "LOOP"}))
        finally:
            device.stop()

    def test_oversized_line_answered_and_stream_resyncs(self):
        device = FakeDevice()
        try:
            service = self.make_service(device)
            huge = b'{"jsonrpc":"2.0","method":"ping","id":1,"pad":"' + b"x" * (70 * 1024) + b'"}\n'
            stdin = io.BytesIO(huge + self.line("ping", {}, 2).encode() + b"\n")
            stdout = io.BytesIO()
            service.run(stdin=stdin, stdout=stdout)
            lines = stdout.getvalue().decode().strip().split("\n")
            self.assertEqual(len(lines), 2)
            oversized = json.loads(lines[0])
            self.assertIsNone(oversized["id"])
            self.assertEqual(oversized["error"]["code"], -32600)
            recovered = json.loads(lines[1])
            self.assertEqual(recovered["id"], 2)
            self.assertTrue(recovered["result"]["pong"])
        finally:
            device.stop()

    def test_notification_executed_but_never_answered(self):
        device = FakeDevice()
        try:
            service = self.make_service(device)
            notification = json.dumps(
                {"jsonrpc": "2.0", "method": "tools/call", "params": {"name": "clear_display"}}
            )
            self.assertIsNone(service.handle_line(notification))
            # The command still executed (JSON-RPC 2.0 §4: no reply, but acted upon).
            self.assertEqual(device.requests[-1], ("POST", "/display/clear", {}))
            # And the service still answers normal requests afterwards.
            response = json.loads(self.service_line_ping(service))
            self.assertTrue(response["result"]["pong"])
        finally:
            device.stop()

    def test_non_utf8_line_is_parse_error(self):
        device = FakeDevice()
        try:
            service = self.make_service(device)
            stdin = io.BytesIO(b"\xff\xfe\x00\n" + self.line("ping", {}, 3).encode() + b"\n")
            stdout = io.BytesIO()
            service.run(stdin=stdin, stdout=stdout)
            lines = stdout.getvalue().decode().strip().split("\n")
            self.assertEqual(len(lines), 2)
            self.assertEqual(json.loads(lines[0])["error"]["code"], -32700)
            self.assertEqual(json.loads(lines[1])["id"], 3)
        finally:
            device.stop()

    @staticmethod
    def service_line_ping(service):
        return service.handle_line(ServiceTestBase.line("ping", {}, "after"))


if __name__ == "__main__":
    unittest.main()
