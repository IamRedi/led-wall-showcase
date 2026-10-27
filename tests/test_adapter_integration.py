"""Deterministic in-process integration round-trip (C-L005-1, V2).

A minimal fake MQTT broker and a fake LED Wall device (HTTP, per
docs/api/CONTRACT.md) run on 127.0.0.1 ephemeral ports; the real adapter,
device client, entity mapping, and MQTT client run against them. This is the
host-verifiable substitute for live V3/V4 checks (real broker + real device),
which remain pending infrastructure/device authorization.
"""

import http.server
import json
import socket
import sys
import threading
import time
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from ha_adapter.adapter import LedWallHaAdapter  # noqa: E402
from ha_adapter.config import AdapterConfig  # noqa: E402
from ha_adapter.device import DeviceClient  # noqa: E402
from ha_adapter.mqtt_proto import MqttClient, encode_publish  # noqa: E402

DEVICE_ID = "led-wall-integration01"

CORE_CAPABILITIES = [
    "display.text.show", "display.text.clear", "display.scene.set",
    "display.animation.set", "display.expression.set", "display.clear",
    "display.brightness.set", "display.state.read",
]

KNOWN_SCENES = ("corners", "checker", "border", "row_sweep", "bounce", "wave", "rain", "plasma")
KNOWN_EXPRESSIONS = ("idle", "listening", "thinking", "speaking", "happy", "alert", "error", "sleeping")


def parse_payload(raw: bytes):
    """MQTT payloads here are JSON or plain strings (e.g. mode/state)."""
    text = raw.decode("utf-8")
    try:
        return json.loads(text)
    except json.JSONDecodeError:
        return text


def wait_until(predicate, timeout_s=8.0, interval_s=0.02):
    deadline = time.monotonic() + timeout_s
    while time.monotonic() < deadline:
        if predicate():
            return True
        time.sleep(interval_s)
    return False


def _read_exact(sock, count):
    data = b""
    while len(data) < count:
        chunk = sock.recv(count - len(data))
        if not chunk:
            raise ConnectionError("closed")
        data += chunk
    return data


def _read_mqtt_packet(sock):
    """Broker-side framer for client packets; returns (first_byte, body)."""
    first = _read_exact(sock, 1)[0]
    multiplier, remaining = 1, 0
    while True:
        byte = _read_exact(sock, 1)[0]
        remaining += (byte & 0x7F) * multiplier
        if not byte & 0x80:
            break
        multiplier *= 128
    body = _read_exact(sock, remaining) if remaining else b""
    return first, body


def _read_utf8(body, pos):
    length = int.from_bytes(body[pos : pos + 2], "big")
    pos += 2
    return body[pos : pos + length].decode("utf-8"), pos + length


# --------------------------------------------------------------------- fake broker ---

class FakeBroker:
    """Accepts MQTT connections, records publishes, delivers injected commands."""

    def __init__(self):
        self._server = socket.socket()
        self._server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self._server.bind(("127.0.0.1", 0))
        self._server.listen(4)
        self.port = self._server.getsockname()[1]
        self._stop = threading.Event()
        self._lock = threading.Lock()
        self.connects = 0
        self.subscriptions = []  # list[(topic, qos)]
        self.publishes = []      # dict(topic, payload, retain, qos)
        self.pubacks = []        # packet ids the client acked for our publishes
        self.client_sock = None
        self._pid = 0
        self._accept_thread = threading.Thread(target=self._accept_loop, daemon=True)
        self._accept_thread.start()

    def _next_pid(self):
        self._pid = (self._pid % 65535) + 1
        return self._pid

    def _accept_loop(self):
        while not self._stop.is_set():
            try:
                sock, _addr = self._server.accept()
            except OSError:
                return
            sock.settimeout(0.2)
            threading.Thread(target=self._serve_client, args=(sock,), daemon=True).start()

    def _serve_client(self, sock):
        with self._lock:
            self.client_sock = sock
            self.connects += 1
        try:
            first, body = _read_mqtt_packet(sock)
            if first >> 4 != 1:  # CONNECT
                return
            sock.sendall(b"\x20\x02\x00\x00")  # CONNACK, rc 0
            while not self._stop.is_set():
                try:
                    first, body = _read_mqtt_packet(sock)
                except socket.timeout:
                    continue
                except (OSError, ConnectionError):
                    return
                ptype = first >> 4
                if ptype == 8:  # SUBSCRIBE
                    pos, topics = 2, []
                    packet_id = int.from_bytes(body[0:2], "big")
                    while pos < len(body):
                        topic, pos = _read_utf8(body, pos)
                        topics.append((topic, body[pos]))
                        pos += 1
                    with self._lock:
                        self.subscriptions.extend(topics)
                    granted = b"\x01" * len(topics)
                    payload = packet_id.to_bytes(2, "big") + granted
                    sock.sendall(b"\x90" + bytes([len(payload)]) + payload)
                elif ptype == 3:  # PUBLISH
                    qos = (first >> 1) & 0x03
                    topic, pos = _read_utf8(body, 0)
                    packet_id = None
                    if qos == 1:
                        packet_id = int.from_bytes(body[pos : pos + 2], "big")
                        pos += 2
                    with self._lock:
                        self.publishes.append({
                            "topic": topic, "payload": body[pos:],
                            "retain": bool(first & 0x01), "qos": qos,
                        })
                    if qos == 1 and packet_id is not None:
                        sock.sendall(b"\x40\x02" + packet_id.to_bytes(2, "big"))
                elif ptype == 4:  # PUBACK (client acking our injected publish)
                    with self._lock:
                        self.pubacks.append(int.from_bytes(body[0:2], "big"))
                elif ptype == 12:  # PINGREQ
                    sock.sendall(b"\xd0\x00")
                elif ptype == 14:  # DISCONNECT
                    return
        finally:
            with self._lock:
                if self.client_sock is sock:
                    self.client_sock = None
            try:
                sock.close()
            except OSError:
                pass

    # -- public API ------------------------------------------------------------

    def send_command(self, topic: str, payload: bytes) -> bool:
        """Inject a QoS 1 PUBLISH; returns False when no client is connected."""
        with self._lock:
            sock = self.client_sock
        if sock is None:
            return False
        sock.sendall(encode_publish(topic, payload, qos=1, retain=False,
                                    packet_id=self._next_pid()))
        return True

    def kick(self) -> None:
        with self._lock:
            sock = self.client_sock
        if sock is not None:
            try:
                sock.close()
            except OSError:
                pass

    def publishes_on(self, topic):
        with self._lock:
            return [dict(p) for p in self.publishes if p["topic"] == topic]

    def stop(self):
        self._stop.set()
        try:
            self._server.close()
        except OSError:
            pass
        self.kick()


# --------------------------------------------------------------------- fake device ---

class _DeviceHandler(http.server.BaseHTTPRequestHandler):
    state = {
        "base_mode": "idle",
        "scene": None,
        "animation": None,
        "expression": None,
        "text": None,
        "brightness": 57,
        "brightness_max": 80,
    }
    commands = []  # list[(path, body)]

    def log_message(self, *args):
        pass

    def _json(self, status, payload):
        body = json.dumps(payload).encode()
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        if self.path == "/identity":
            self._json(200, {
                "deviceId": DEVICE_ID,
                "deviceType": "visual_display_v1",
                "role": "display",
                "protocolVersion": 1,
                "fwVersion": "0.1.0-integration",
                "capabilities": CORE_CAPABILITIES,
                "matrix": {"width": 30, "height": 10},
            })
            return
        if self.path == "/state":
            with _DeviceHandler.lock:
                self._json(200, {
                    "available": True,
                    "deviceId": DEVICE_ID,
                    "deviceType": "visual_display_v1",
                    "protocolVersion": 1,
                    "fwVersion": "0.1.0-integration",
                    "baseMode": _DeviceHandler.state["base_mode"],
                    "currentMode": _DeviceHandler.state["base_mode"],
                    "activeTransient": None,
                    "activeScene": _DeviceHandler.state["scene"],
                    "activeAnimation": _DeviceHandler.state["animation"],
                    "activeExpression": _DeviceHandler.state["expression"],
                    "text": None if _DeviceHandler.state["text"] is None
                            else {"current": _DeviceHandler.state["text"], "scrolling": False},
                    "brightness": {"current": _DeviceHandler.state["brightness"],
                                   "max": _DeviceHandler.state["brightness_max"]},
                    "spectrum": {"available": False, "active": False},
                    "matrix": {"width": 30, "height": 10},
                    "clients": {"count": 1},
                    "warnings": [],
                    "uptimeSeconds": 5,
                })
            return
        self._json(404, {"ok": False, "error": {"code": "unsupportedCapability",
                                                "message": "unknown path"}})

    def do_POST(self):
        length = int(self.headers.get("Content-Length", "0"))
        raw = self.rfile.read(length)
        try:
            body = json.loads(raw.decode("utf-8")) if raw else {}
        except (json.JSONDecodeError, UnicodeDecodeError):
            self._json(400, {"ok": False, "error": {"code": "invalidParameter",
                                                    "message": "bad json"}})
            return
        path = self.path
        with _DeviceHandler.lock:
            _DeviceHandler.commands.append((path, body))
            state = _DeviceHandler.state
            if path == "/display/brightness/set":
                level = body.get("level")
                if isinstance(level, bool) or not isinstance(level, (int, float)):
                    self._json(400, {"ok": False, "error": {"code": "invalidParameter",
                                                            "message": "level"}})
                    return
                state["brightness"] = max(0, min(int(level), state["brightness_max"]))
                self._json(200, {"ok": True})
                return
            if path == "/display/scene/set":
                if body.get("scene") not in KNOWN_SCENES:
                    self._json(400, {"ok": False, "error": {
                        "code": "unknownScene", "message": "unknown scene",
                        "available": ", ".join(KNOWN_SCENES)}})
                    return
                state["base_mode"] = "scene"
                state["scene"] = body["scene"]
                self._json(200, {"ok": True})
                return
            if path == "/display/animation/set":
                if body.get("animation") not in KNOWN_SCENES:
                    self._json(400, {"ok": False, "error": {
                        "code": "unknownAnimation", "message": "unknown animation"}})
                    return
                state["base_mode"] = "animation"
                state["animation"] = body["animation"]
                self._json(200, {"ok": True})
                return
            if path == "/display/expression/set":
                if body.get("expression") not in KNOWN_EXPRESSIONS:
                    self._json(400, {"ok": False, "error": {
                        "code": "unknownExpression", "message": "unknown expression",
                        "available": ", ".join(KNOWN_EXPRESSIONS)}})
                    return
                state["base_mode"] = "expression"
                state["expression"] = body["expression"]
                self._json(200, {"ok": True})
                return
            if path == "/display/text/show":
                text = body.get("text")
                if not isinstance(text, str) or not (1 <= len(text) <= 128):
                    self._json(400, {"ok": False, "error": {
                        "code": "invalidParameter", "message": "text"}})
                    return
                state["base_mode"] = "text"
                state["text"] = text
                self._json(200, {"ok": True})
                return
            if path == "/display/clear":
                state["base_mode"] = "idle"
                state["scene"] = None
                state["animation"] = None
                state["expression"] = None
                state["text"] = None
                self._json(200, {"ok": True})
                return
        self._json(404, {"ok": False, "error": {"code": "unsupportedCapability",
                                                "message": "unknown path"}})


_DeviceHandler.lock = threading.Lock()


class IntegrationFixture:
    def __init__(self):
        _DeviceHandler.commands = []
        with _DeviceHandler.lock:
            _DeviceHandler.state.update({
                "base_mode": "idle", "scene": None, "animation": None,
                "expression": None, "text": None, "brightness": 57,
                "brightness_max": 80,
            })
        self.device = http.server.ThreadingHTTPServer(("127.0.0.1", 0), _DeviceHandler)
        self.device_port = self.device.server_address[1]
        threading.Thread(target=self.device.serve_forever, daemon=True).start()

        self.broker = FakeBroker()
        self.config = AdapterConfig(
            device_url=f"http://127.0.0.1:{self.device_port}",
            mqtt_host="127.0.0.1",
            mqtt_port=self.broker.port,
            poll_interval_s=0.1,
            device_timeout_s=1.0,
            keepalive_s=10,
        )
        self.adapter = LedWallHaAdapter(
            self.config,
            device=DeviceClient(self.config.device_url, self.config.device_timeout_s),
            mqtt_factory=lambda **kwargs: MqttClient(**{**kwargs, "backoff_base_s": 0.1}),
        )
        self.thread = threading.Thread(target=self.adapter.run, daemon=True)
        self.thread.start()

    def topic(self, suffix):
        return f"ledwall/{DEVICE_ID}/{suffix}"

    def payload_of(self, topic, index=-1):
        records = self.broker.publishes_on(topic)
        if not records:
            return None
        return parse_payload(records[index]["payload"])

    def raw_payload_of(self, topic, index=-1):
        records = self.broker.publishes_on(topic)
        if not records:
            return None
        return records[index]["payload"].decode("utf-8")

    def stop(self):
        self.adapter.stop()
        self.thread.join(timeout=5.0)
        self.broker.stop()
        self.device.shutdown()
        self.device.server_close()


class IntegrationTest(unittest.TestCase):
    def setUp(self):
        self.fx = IntegrationFixture()

    def tearDown(self):
        self.fx.stop()

    def wait_for_payload(self, topic, predicate, timeout_s=8.0):
        deadline = time.monotonic() + timeout_s
        while time.monotonic() < deadline:
            records = self.fx.broker.publishes_on(topic)
            for record in records:
                payload = parse_payload(record["payload"])
                if predicate(payload):
                    return payload
            time.sleep(0.02)
        self.fail(f"no matching payload on {topic} within {timeout_s}s")

    def test_discovery_and_initial_state(self):
        self.assertTrue(wait_until(lambda: self.fx.broker.connects >= 1))
        discovery = f"homeassistant/light/{DEVICE_ID}/display/config"
        self.assertTrue(
            wait_until(lambda: self.fx.broker.publishes_on(discovery)),
            "discovery config for the light was not published",
        )
        config = self.fx.payload_of(discovery)
        self.assertEqual(config["brightness_scale"], 100)
        self.assertEqual(config["state_topic"], self.fx.topic("light/state"))
        self.assertEqual(config["unique_id"], f"led_wall_{DEVICE_ID}_display")
        self.assertTrue(
            wait_until(lambda: self.fx.broker.subscriptions),
            "command subscriptions missing",
        )
        subscribed = {topic for topic, _qos in self.fx.broker.subscriptions}
        for suffix in ("light/command", "scene/command", "animation/command",
                       "expression/command", "text/command", "clear/command"):
            self.assertIn(self.fx.topic(suffix), subscribed)
        light = self.wait_for_payload(self.fx.topic("light/state"),
                                      lambda p: p["state"] == "ON")
        self.assertEqual(light["brightness"], 57)
        self.assertEqual(self.wait_for_payload(self.fx.topic("mode/state"),
                                               lambda p: p == "idle"), "idle")
        avail = self.fx.raw_payload_of(self.fx.topic("availability"))
        self.assertEqual(avail, "online")
        device_avail = self.fx.raw_payload_of(self.fx.topic("device_availability"))
        self.assertEqual(device_avail, "online")

    def test_light_command_round_trip(self):
        self.assertTrue(wait_until(lambda: self.fx.broker.subscriptions))
        self.assertTrue(self.fx.broker.send_command(
            self.fx.topic("light/command"), b'{"state":"ON","brightness":23}'))
        self.assertTrue(
            wait_until(lambda: any(p == "/display/brightness/set" and b.get("level") == 23
                                   for p, b in _DeviceHandler.commands)),
            "brightness command did not reach the device",
        )
        self.wait_for_payload(self.fx.topic("light/state"),
                              lambda p: p["state"] == "ON" and p["brightness"] == 23)

        self.assertTrue(self.fx.broker.send_command(
            self.fx.topic("light/command"), b'{"state":"OFF"}'))
        self.assertTrue(
            wait_until(lambda: any(p == "/display/brightness/set" and b.get("level") == 0
                                   for p, b in _DeviceHandler.commands)))
        self.wait_for_payload(self.fx.topic("light/state"), lambda p: p["state"] == "OFF")

        # ON without brightness restores the last seen nonzero level (23).
        self.assertTrue(self.fx.broker.send_command(
            self.fx.topic("light/command"), b'{"state":"ON"}'))
        self.assertTrue(
            wait_until(lambda: any(p == "/display/brightness/set" and b.get("level") == 23
                                   for p, b in _DeviceHandler.commands)))

    def test_invalid_light_command_never_reaches_device(self):
        self.assertTrue(wait_until(lambda: self.fx.broker.subscriptions))
        before = len(_DeviceHandler.commands)
        self.assertTrue(self.fx.broker.send_command(
            self.fx.topic("light/command"), b'{"state":"MAYBE"}'))
        time.sleep(0.5)
        self.assertTrue(self.fx.broker.send_command(
            self.fx.topic("light/command"), b'garbage'))
        time.sleep(0.5)
        self.assertEqual(len(_DeviceHandler.commands), before)

    def test_scene_expression_text_clear_round_trip(self):
        self.assertTrue(wait_until(lambda: self.fx.broker.subscriptions))

        self.assertTrue(self.fx.broker.send_command(self.fx.topic("scene/command"), b"wave"))
        self.assertTrue(wait_until(lambda: any(
            p == "/display/scene/set" and b == {"scene": "wave"}
            for p, b in _DeviceHandler.commands)))
        self.wait_for_payload(self.fx.topic("scene/state"), lambda p: p == "wave")

        # Unknown expression: device typed error, adapter keeps running, no state.
        self.assertTrue(self.fx.broker.send_command(
            self.fx.topic("expression/command"), b"nope"))
        self.assertTrue(wait_until(lambda: any(
            p == "/display/expression/set" and b == {"expression": "nope"}
            for p, b in _DeviceHandler.commands)))
        time.sleep(0.3)
        self.assertTrue(self.fx.thread.is_alive())
        # Rejected command mutates nothing: state stays the initial empty string.
        self.assertEqual(self.fx.payload_of(self.fx.topic("expression/state")), "")

        self.assertTrue(self.fx.broker.send_command(
            self.fx.topic("expression/command"), b"alert"))
        self.wait_for_payload(self.fx.topic("expression/state"), lambda p: p == "alert")
        mode = self.wait_for_payload(self.fx.topic("mode/state"), lambda p: p == "expression")
        self.assertEqual(mode, "expression")

        self.assertTrue(self.fx.broker.send_command(self.fx.topic("text/command"), b"HELLO"))
        self.wait_for_payload(self.fx.topic("text/state"), lambda p: p == "HELLO")

        self.assertTrue(self.fx.broker.send_command(self.fx.topic("clear/command"), b"press"))
        self.assertTrue(wait_until(lambda: any(
            p == "/display/clear" for p, _b in _DeviceHandler.commands)))
        self.wait_for_payload(self.fx.topic("mode/state"), lambda p: p == "idle")
        self.wait_for_payload(self.fx.topic("scene/state"), lambda p: p == "")

    def test_device_outage_flips_availability(self):
        self.assertTrue(wait_until(lambda: self.fx.broker.subscriptions))
        self.wait_for_payload(self.fx.topic("mode/state"), lambda p: isinstance(p, str))
        self.fx.device.shutdown()
        self.fx.device.server_close()
        self.assertTrue(
            wait_until(lambda: self.fx.raw_payload_of(
                self.fx.topic("device_availability")) == "offline",
                timeout_s=15.0),
            "device_availability did not flip to offline during outage",
        )

    def test_broker_reconnect_republishes_state(self):
        self.assertTrue(wait_until(lambda: self.fx.broker.connects >= 1))
        self.wait_for_payload(self.fx.topic("mode/state"), lambda p: p == "idle")
        publishes_before = len(self.fx.broker.publishes)
        self.fx.broker.kick()
        self.assertTrue(
            wait_until(lambda: self.fx.broker.connects >= 2, timeout_s=15.0),
            "adapter did not reconnect to the broker",
        )
        self.assertTrue(
            wait_until(lambda: len(self.fx.broker.publishes) > publishes_before),
            "no state republished after reconnect",
        )
        self.wait_for_payload(self.fx.topic("mode/state"), lambda p: p == "idle")
        self.assertTrue(self.fx.thread.is_alive())


if __name__ == "__main__":
    unittest.main()
