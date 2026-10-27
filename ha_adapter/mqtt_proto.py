"""Minimal MQTT 3.1.1 client subset (bounded, stdlib-only).

Deliberate scope (ADR-LED-L005 decision 2): CONNECT/CONNACK, PUBLISH QoS 0/1
in both directions, SUBSCRIBE QoS 1, PUBACK, PINGREQ/PINGRESP, DISCONNECT,
last-will, `clean_session=false` with a stable client id so the broker queues
QoS 1 commands while the adapter restarts. No QoS 2, no TLS in v1 (documented
LAN-trust posture). This module is an internal, replaceable component.

Boundedness: total in-flight publishes are capped, pending publishes are
coalesced per topic (latest wins), incoming packets above a hard size limit
kill the connection, and reconnect uses capped exponential backoff on the
monotonic clock.
"""

from __future__ import annotations

import socket
import threading
import time
from dataclasses import dataclass
from typing import Callable, Dict, List, Optional, Tuple

# Fixed header type values (MQTT 3.1.2.1).
_CONNECT = 1
_CONNACK = 2
_PUBLISH = 3
_PUBACK = 4
_SUBSCRIBE = 8
_SUBACK = 9
_PINGREQ = 12
_PINGRESP = 13
_DISCONNECT = 14

_MAX_REMAINING_LENGTH = 268_435_455  # 4-byte varint maximum (spec §2.2.3)
_MAX_INCOMING_PACKET = 1 * 1024 * 1024
_CONNECT_TIMEOUT_S = 5.0
_CONNACK_TIMEOUT_S = 10.0
_PUBACK_TIMEOUT_S = 10.0

_CONNACK_RC = {
    1: "unacceptable protocol version",
    2: "identifier rejected",
    3: "server unavailable",
    4: "bad user name or password",
    5: "not authorized",
}


class MqttProtocolError(Exception):
    """A peer packet violated the protocol or local bounds — reconnect."""


@dataclass(frozen=True)
class Connack:
    session_present: bool
    return_code: int


@dataclass(frozen=True)
class Suback:
    packet_id: int
    return_codes: Tuple[int, ...]


@dataclass(frozen=True)
class Publish:
    topic: str
    payload: bytes
    qos: int
    retain: bool
    dup: bool
    packet_id: Optional[int]


@dataclass(frozen=True)
class Puback:
    packet_id: int


@dataclass(frozen=True)
class Pingresp:
    pass


# ---------------------------------------------------------------- encoding ---

def _encode_utf8(value: str) -> bytes:
    raw = value.encode("utf-8")
    if len(raw) > 65535:
        raise MqttProtocolError("UTF-8 string exceeds 65535 bytes")
    return len(raw).to_bytes(2, "big") + raw


def _encode_binary(value: bytes) -> bytes:
    if len(value) > 65535:
        raise MqttProtocolError("binary field exceeds 65535 bytes")
    return len(value).to_bytes(2, "big") + value


def encode_remaining_length(value: int) -> bytes:
    if value < 0 or value > _MAX_REMAINING_LENGTH:
        raise MqttProtocolError(f"remaining length out of range: {value}")
    out = bytearray()
    while True:
        digit = value % 128
        value //= 128
        if value > 0:
            digit |= 0x80
        out.append(digit)
        if value == 0:
            return bytes(out)


def _packet(first_byte: int, body: bytes) -> bytes:
    return bytes([first_byte]) + encode_remaining_length(len(body)) + body


def encode_connect(
    client_id: str,
    keepalive_s: int,
    clean_session: bool,
    username: Optional[str] = None,
    password: Optional[str] = None,
    will_topic: Optional[str] = None,
    will_payload: bytes = b"",
    will_retain: bool = False,
    will_qos: int = 0,
) -> bytes:
    if not client_id:
        raise MqttProtocolError("client_id must be non-empty")
    if isinstance(will_payload, str):  # accept str for caller convenience
        will_payload = will_payload.encode("utf-8")
    flags = 0
    if clean_session:
        flags |= 0x02
    payload = _encode_utf8(client_id)
    if will_topic is not None:
        if will_qos not in (0, 1):
            raise MqttProtocolError("will QoS must be 0 or 1")
        flags |= 0x04 | (will_qos << 3)
        if will_retain:
            flags |= 0x20
        payload += _encode_utf8(will_topic) + _encode_binary(will_payload)
    if username is not None:
        flags |= 0x80
        payload += _encode_utf8(username)
    if password is not None:
        flags |= 0x40
        payload += _encode_utf8(password)
    variable = _encode_utf8("MQTT") + bytes([0x04, flags]) + int(keepalive_s).to_bytes(2, "big")
    return _packet(_CONNECT << 4, variable + payload)


def encode_subscribe(packet_id: int, topics: List[Tuple[str, int]]) -> bytes:
    if not topics:
        raise MqttProtocolError("subscribe requires at least one topic")
    body = int(packet_id).to_bytes(2, "big")
    for topic, qos in topics:
        if qos not in (0, 1):
            raise MqttProtocolError("subscribe QoS must be 0 or 1")
        body += _encode_utf8(topic) + bytes([qos])
    return _packet((_SUBSCRIBE << 4) | 0x02, body)  # fixed header flags must be 0010


def encode_publish(
    topic: str, payload: bytes, qos: int, retain: bool, packet_id: Optional[int], dup: bool = False
) -> bytes:
    if qos not in (0, 1):
        raise MqttProtocolError("publish QoS must be 0 or 1")
    first = _PUBLISH << 4
    if dup:
        first |= 0x08
    if qos == 1:
        first |= 0x02
    if retain:
        first |= 0x01
    body = _encode_utf8(topic)
    if qos == 1:
        if packet_id is None:
            raise MqttProtocolError("QoS 1 publish requires a packet id")
        body += int(packet_id).to_bytes(2, "big")
    return _packet(first, body + payload)


def encode_puback(packet_id: int) -> bytes:
    return _packet(_PUBACK << 4, int(packet_id).to_bytes(2, "big"))


def encode_pingreq() -> bytes:
    return _packet(_PINGREQ << 4, b"")


def encode_disconnect() -> bytes:
    return _packet(_DISCONNECT << 4, b"")


# ---------------------------------------------------------------- decoding ---

def decode_remaining_length(data: bytes, pos: int) -> Tuple[int, int]:
    multiplier = 1
    value = 0
    while True:
        if pos >= len(data):
            raise IndexError("need more bytes for remaining length")
        byte = data[pos]
        pos += 1
        value += (byte & 0x7F) * multiplier
        if not byte & 0x80:
            return value, pos
        multiplier *= 128
        if multiplier > 128 ** 3:
            raise MqttProtocolError("remaining length varint exceeds 4 bytes")


def _read_utf8(data: bytes, pos: int) -> Tuple[str, int]:
    if pos + 2 > len(data):
        raise MqttProtocolError("truncated UTF-8 string length")
    length = int.from_bytes(data[pos : pos + 2], "big")
    pos += 2
    if pos + length > len(data):
        raise MqttProtocolError("truncated UTF-8 string body")
    try:
        return data[pos : pos + length].decode("utf-8"), pos + length
    except UnicodeDecodeError as exc:
        raise MqttProtocolError("invalid UTF-8 in string") from exc


def decode_packet(first_byte: int, body: bytes):
    packet_type = first_byte >> 4
    if packet_type == _CONNACK:
        if len(body) != 2:
            raise MqttProtocolError("CONNACK must carry exactly 2 bytes")
        return Connack(session_present=bool(body[0] & 0x01), return_code=body[1])
    if packet_type == _SUBACK:
        if len(body) < 3:
            raise MqttProtocolError("SUBACK too short")
        packet_id = int.from_bytes(body[0:2], "big")
        codes = tuple(body[2:])
        for code in codes:
            if code not in (0x00, 0x01, 0x02, 0x80):
                raise MqttProtocolError(f"invalid SUBACK return code {code:#x}")
        return Suback(packet_id=packet_id, return_codes=codes)
    if packet_type == _PUBLISH:
        qos = (first_byte >> 1) & 0x03
        if qos == 3:
            raise MqttProtocolError("PUBLISH QoS 3 is invalid")
        retain = bool(first_byte & 0x01)
        dup = bool(first_byte & 0x08)
        topic, pos = _read_utf8(body, 0)
        if ("+" in topic) or ("#" in topic):
            raise MqttProtocolError("wildcard in PUBLISH topic")
        packet_id = None
        if qos == 1:
            if pos + 2 > len(body):
                raise MqttProtocolError("QoS 1 PUBLISH missing packet id")
            packet_id = int.from_bytes(body[pos : pos + 2], "big")
            pos += 2
        return Publish(topic=topic, payload=body[pos:], qos=qos, retain=retain, dup=dup, packet_id=packet_id)
    if packet_type == _PUBACK:
        if len(body) != 2:
            raise MqttProtocolError("PUBACK must carry exactly 2 bytes")
        return Puback(packet_id=int.from_bytes(body, "big"))
    if packet_type == _PINGRESP:
        return Pingresp()
    raise MqttProtocolError(f"unexpected packet type {packet_type} from broker")


class PacketReader:
    """Incremental, bounded packet framer: feed() -> zero or more packets."""

    def __init__(self, max_packet_size: int = _MAX_INCOMING_PACKET):
        self._buffer = bytearray()
        self._max = max_packet_size

    def feed(self, data: bytes) -> List[object]:
        self._buffer.extend(data)
        # Hard memory bound: a header claiming more than the cap, or an
        # oversized unframed buffer, is fatal for the connection.
        packets: List[object] = []
        while True:
            if len(self._buffer) < 2:
                return packets
            try:
                remaining, body_start = decode_remaining_length(bytes(self._buffer), 1)
            except IndexError:
                return packets  # varint header incomplete; need more bytes
            if remaining > self._max:
                raise MqttProtocolError(f"incoming packet exceeds {self._max} byte cap")
            total = body_start + remaining
            if len(self._buffer) < total:
                return packets
            first = self._buffer[0]
            body = bytes(self._buffer[body_start:total])
            del self._buffer[:total]
            packets.append(decode_packet(first, body))


# ------------------------------------------------------------------ client ---

OnMessage = Callable[[str, bytes], None]
OnConnected = Callable[[], None]
OnDisconnected = Callable[[str], None]


class MqttClient:
    """Resilient MQTT connection with a bounded QoS 1 publish pipeline."""

    def __init__(
        self,
        host: str,
        port: int,
        client_id: str,
        *,
        keepalive_s: int = 30,
        username: Optional[str] = None,
        password: Optional[str] = None,
        will_topic: Optional[str] = None,
        will_payload: bytes = b"offline",
        will_retain: bool = True,
        clean_session: bool = False,
        max_inflight: int = 8,
        backoff_base_s: float = 1.0,
        backoff_max_s: float = 30.0,
        on_message: Optional[OnMessage] = None,
        on_connected: Optional[OnConnected] = None,
        on_disconnected: Optional[OnDisconnected] = None,
    ):
        self._host = host
        self._port = port
        self._client_id = client_id
        self._keepalive_s = keepalive_s
        self._username = username
        self._password = password
        self._will_topic = will_topic
        self._will_payload = will_payload
        self._will_retain = will_retain
        self._clean_session = clean_session
        self._max_inflight = max_inflight
        self._backoff_base_s = backoff_base_s
        self._backoff_max_s = backoff_max_s
        self._on_message = on_message
        self._on_connected = on_connected
        self._on_disconnected = on_disconnected

        self._subscriptions: List[Tuple[str, int]] = []
        self._stop = threading.Event()
        self._thread: Optional[threading.Thread] = None
        self._lock = threading.Lock()
        self._sock: Optional[socket.socket] = None
        self._connected = False
        self._packet_id = 0
        self._inflight: Dict[int, Tuple[str, bytes, bool, float]] = {}
        self._pending: Dict[str, Tuple[bytes, bool]] = {}

    # -- public API (thread-safe) --------------------------------------------

    def start(self) -> None:
        if self._thread is not None:
            return
        self._thread = threading.Thread(target=self._run, name="ledwall-mqtt", daemon=True)
        self._thread.start()

    def stop(self, join_timeout_s: float = 5.0) -> None:
        self._stop.set()
        with self._lock:
            sock = self._sock
            if sock is not None:
                try:
                    sock.close()
                except OSError:
                    pass
        thread = self._thread
        if thread is not None and thread.is_alive():
            thread.join(timeout=join_timeout_s)

    def subscribe(self, topic: str, qos: int = 1) -> None:
        self._subscriptions.append((topic, qos))
        with self._lock:
            if not self._connected:
                return
            packet = encode_subscribe(self._next_packet_id(), [(topic, qos)])
            try:
                self._sock.sendall(packet)
            except OSError:
                pass  # reader loop will notice and reconnect

    def publish(self, topic: str, payload: bytes, qos: int = 1, retain: bool = False) -> None:
        """Publish; if not connected or the pipeline is full, coalesce per topic."""
        if isinstance(payload, str):  # accept str for caller convenience
            payload = payload.encode("utf-8")
        if qos == 0:
            with self._lock:
                sock = self._sock if self._connected else None
                if sock is None:
                    return
                try:
                    sock.sendall(encode_publish(topic, payload, 0, retain, None))
                except OSError:
                    pass
            return
        with self._lock:
            if not self._connected or len(self._inflight) >= self._max_inflight:
                self._pending[topic] = (payload, retain)  # latest wins; topic set is fixed
                return
            self._send_publish_locked(topic, payload, retain, dup=False)

    def is_connected(self) -> bool:
        with self._lock:
            return self._connected

    # -- internals -------------------------------------------------------------

    def _next_packet_id(self) -> int:
        self._packet_id = (self._packet_id % 65535) + 1
        return self._packet_id

    def _send_publish_locked(self, topic: str, payload: bytes, retain: bool, dup: bool) -> None:
        pid = self._next_packet_id()
        packet = encode_publish(topic, payload, 1, retain, pid, dup=dup)
        self._inflight[pid] = (topic, payload, retain, time.monotonic())
        self._sock.sendall(packet)  # caller holds the lock; OSError propagates

    def _flush_pending_locked(self) -> None:
        while self._pending and len(self._inflight) < self._max_inflight:
            topic, (payload, retain) = self._pending.popitem()
            self._send_publish_locked(topic, payload, retain, dup=False)

    def _run(self) -> None:
        backoff = self._backoff_base_s
        while not self._stop.is_set():
            reason = ""
            served_since = time.monotonic()
            try:
                self._serve()
                reason = "stopped"
            except MqttConnectRefused as exc:
                reason = f"refused: {_CONNACK_RC.get(exc.return_code, f'rc={exc.return_code}')}"
            except (MqttProtocolError, OSError) as exc:
                reason = f"{type(exc).__name__}: {exc}"
            finally:
                self._teardown_connection(reason)
            if self._stop.is_set():
                return
            # A connection that lived a while restarts the backoff sequence.
            if time.monotonic() - served_since > self._backoff_max_s:
                backoff = self._backoff_base_s
            time.sleep(min(backoff, self._backoff_max_s))
            backoff = min(backoff * 2, self._backoff_max_s)

    def _serve(self) -> None:
        sock = socket.create_connection((self._host, self._port), timeout=_CONNECT_TIMEOUT_S)
        sock.settimeout(0.5)
        with self._lock:
            self._sock = sock
        sock.sendall(
            encode_connect(
                self._client_id,
                self._keepalive_s,
                self._clean_session,
                username=self._username,
                password=self._password,
                will_topic=self._will_topic,
                will_payload=self._will_payload,
                will_retain=self._will_retain,
                will_qos=0,
            )
        )
        connack = self._await_connack(sock)
        with self._lock:
            self._connected = True
        if self._on_connected is not None:
            self._safe_callback(self._on_connected)
        if self._subscriptions:
            with self._lock:
                sock.sendall(encode_subscribe(self._next_packet_id(), list(self._subscriptions)))
        with self._lock:
            self._flush_pending_locked()
        self._read_loop(sock)

    def _await_connack(self, sock: socket.socket) -> Connack:
        reader = PacketReader()
        deadline = time.monotonic() + _CONNACK_TIMEOUT_S
        while True:
            if time.monotonic() > deadline:
                raise MqttProtocolError("no CONNACK within timeout")
            try:
                data = sock.recv(4096)
            except socket.timeout:
                continue
            if data == b"":
                raise MqttProtocolError("connection closed before CONNACK")
            for packet in reader.feed(data):
                if isinstance(packet, Connack):
                    if packet.return_code != 0:
                        raise MqttConnectRefused(packet.return_code)
                    return packet
                raise MqttProtocolError("first packet was not CONNACK")

    def _read_loop(self, sock: socket.socket) -> None:
        reader = PacketReader()
        last_outgoing = time.monotonic()
        outstanding_ping = False
        ping_deadline = 0.0
        ping_interval = max(1.0, self._keepalive_s / 2.0)
        while not self._stop.is_set():
            now = time.monotonic()
            try:
                data = sock.recv(4096)
            except socket.timeout:
                data = None
            except OSError as exc:
                raise MqttProtocolError(f"socket error: {exc}") from exc
            if data == b"":
                raise MqttProtocolError("connection closed by broker")
            if data:
                for packet in reader.feed(data):
                    if isinstance(packet, Publish):
                        if packet.qos == 1 and packet.packet_id is not None:
                            with self._lock:
                                sock.sendall(encode_puback(packet.packet_id))
                        if self._on_message is not None:
                            self._safe_message(packet.topic, packet.payload)
                    elif isinstance(packet, Puback):
                        with self._lock:
                            self._inflight.pop(packet.packet_id, None)
                            self._flush_pending_locked()
                    elif isinstance(packet, Suback):
                        if any(code == 0x80 for code in packet.return_codes):
                            self._log_warning(
                                f"broker rejected a subscription (SUBACK 0x80, id {packet.packet_id})"
                            )
                    elif isinstance(packet, Pingresp):
                        outstanding_ping = False
            if now - last_outgoing >= ping_interval and not outstanding_ping:
                with self._lock:
                    sock.sendall(encode_pingreq())
                last_outgoing = now
                outstanding_ping = True
                ping_deadline = now + self._keepalive_s
            if outstanding_ping and now > ping_deadline:
                raise MqttProtocolError("no PINGRESP within keepalive window")
            with self._lock:
                expired = [
                    pid
                    for pid, (_t, _p, _r, sent) in self._inflight.items()
                    if now - sent > _PUBACK_TIMEOUT_S
                ]
                if expired:
                    raise MqttProtocolError(f"PUBACK timeout for packet ids {sorted(expired)}")
                if self._pending and len(self._inflight) < self._max_inflight:
                    self._flush_pending_locked()

    def _teardown_connection(self, reason: str) -> None:
        with self._lock:
            was_connected = self._connected
            self._connected = False
            sock = self._sock
            self._sock = None
            # Unacknowledged publishes are dropped here: the adapter republishes
            # its full retained state map on every (re)connect via on_connected,
            # so nothing depends on send-side queue survival across reconnects.
            # Never-sent pending entries stay queued for the next connection.
            self._inflight.clear()
        if sock is not None:
            try:
                if was_connected and not self._stop.is_set():
                    sock.sendall(encode_disconnect())
            except OSError:
                pass
            try:
                sock.close()
            except OSError:
                pass
        if was_connected and not self._stop.is_set() and self._on_disconnected is not None:
            self._safe_disconnected(reason or "connection lost")

    # -- callback safety ------------------------------------------------------

    def _safe_callback(self, fn: Callable[[], None]) -> None:
        try:
            fn()
        except Exception as exc:  # noqa: BLE001 — a callback bug must not kill the link
            self._log_warning(f"on_connected callback failed: {exc}")

    def _safe_message(self, topic: str, payload: bytes) -> None:
        try:
            self._on_message(topic, payload)
        except Exception as exc:  # noqa: BLE001
            self._log_warning(f"message handler failed for {topic!r}: {exc}")

    def _safe_disconnected(self, reason: str) -> None:
        try:
            self._on_disconnected(reason)
        except Exception as exc:  # noqa: BLE001
            self._log_warning(f"on_disconnected callback failed: {exc}")

    @staticmethod
    def _log_warning(message: str) -> None:
        import logging

        logging.getLogger("ledwall_ha.mqtt").warning("%s", message)


class MqttConnectRefused(MqttProtocolError):
    def __init__(self, return_code: int):
        super().__init__(f"CONNACK return code {return_code}")
        self.return_code = return_code
