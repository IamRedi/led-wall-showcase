"""Adapter service loop: LED Wall HTTP polling <-> Home Assistant MQTT.

Ownership model (device stays authoritative, ADR-LED-L005 decision 4):
- The adapter owns only MQTT plumbing state: the last published payloads (for
  change detection + retained republish on reconnect) and the restore level
  (last seen nonzero brightness, used by the light's ON toggle; heritage
  default when none seen). Neither is ever presented as device state.
- Device state is polled `GET /state`; MQTT commands map through
  :mod:`ha_adapter.entities` into §5 API commands. Invalid MQTT input never
  reaches the device; device typed errors never mutate adapter state.

Threading: one MQTT thread (protocol client) + one poll thread. Shared state
is confined to the client (internally locked) and the small attributes noted
below; cross-thread wakes use events on the monotonic clock.
"""

from __future__ import annotations

import json
import logging
import threading
import time
from typing import Callable, Dict, Optional

from .config import AdapterConfig
from .device import (
    DeviceClient,
    DeviceError,
    DeviceIdentity,
    DeviceProtocolError,
    DeviceState,
    DeviceUnreachable,
    TypedDeviceError,
)
from .entities import (
    Topics,
    build_discovery_configs,
    build_state_payloads,
    entity_set_for,
    parse_command,
    state_topic_map,
)
from .mqtt_proto import MqttClient

logger = logging.getLogger("ledwall_ha")

_WAKE_MIN_GAP_S = 0.25  # rate limit for command-triggered immediate re-polls
_BACKOFF_BASE_S = 1.0
_BACKOFF_MAX_S = 30.0


class AdapterStopped(RuntimeError):
    """The adapter was stopped while waiting for the device."""


class LedWallHaAdapter:
    def __init__(
        self,
        config: AdapterConfig,
        device: Optional[DeviceClient] = None,
        mqtt_factory: Optional[Callable[..., MqttClient]] = None,
    ):
        self._config = config
        self._device = device or DeviceClient(config.device_url, config.device_timeout_s)
        self._mqtt_factory = mqtt_factory
        self._stop = threading.Event()
        self._wake = threading.Event()
        self._last_poll_at = 0.0
        self._next_due = 0.0
        # Restore level: last seen nonzero brightness, used by the light ON
        # toggle. Cross-thread int writes are atomic under the GIL; it is
        # advisory control input, never published as device state.
        self._restore_level = 0
        self._device_online: Optional[bool] = None
        self._published: Dict[str, str] = {}  # topic -> last payload (change detection)
        self._topics: Optional[Topics] = None
        self._mqtt: Optional[MqttClient] = None
        self._last_state: Optional[DeviceState] = None
        self._last_identity: Optional[DeviceIdentity] = None

    # -- lifecycle -----------------------------------------------------------

    def run(self) -> None:
        """Block until stop() is called."""
        config = self._config
        for warning in config.warnings:
            logger.warning("config: %s", warning)
        identity = self._wait_for_identity()
        self._topics = Topics(config.topic_prefix, identity.device_id)
        entity_set = entity_set_for(identity)
        logger.info(
            "device %s (fw %s) capabilities: %s",
            identity.device_id,
            identity.fw_version,
            ", ".join(identity.capabilities) or "(none)",
        )
        for capability in ("color", "spectrum", "display.paint.pixel", "display.paint.frame"):
            if capability in identity.capabilities:
                logger.warning(
                    "device advertises capability '%s' but adapter v1 has no control for it",
                    capability,
                )

        self._mqtt = self._build_mqtt(identity.device_id, entity_set)
        for topic, _kind in entity_set.command_topics(self._topics):
            self._mqtt.subscribe(topic)
        self._mqtt.start()
        try:
            self._poll_loop(entity_set)
        finally:
            if self._mqtt is not None:
                self._mqtt.stop()
        logger.info("adapter stopped")

    def stop(self) -> None:
        self._stop.set()
        self._wake.set()

    # -- MQTT wiring -----------------------------------------------------------

    def _build_mqtt(self, device_id: str, entity_set) -> MqttClient:
        topics = self._topics
        assert topics is not None
        command_map = dict(entity_set.command_topics(topics))
        kwargs = dict(
            host=self._config.mqtt_host,
            port=self._config.mqtt_port,
            client_id=f"ledwall-ha-{device_id}",
            keepalive_s=self._config.keepalive_s,
            username=self._config.mqtt_username,
            password=self._config.mqtt_password,
            will_topic=topics.availability,
            will_payload=b"offline",
            on_message=lambda topic, payload: self._on_mqtt_message(command_map, topic, payload),
            on_connected=self._on_mqtt_connected,
            on_disconnected=lambda reason: logger.warning("MQTT disconnected: %s", reason),
        )
        if self._mqtt_factory is not None:
            return self._mqtt_factory(**kwargs)
        return MqttClient(**kwargs)

    def _on_mqtt_connected(self) -> None:
        topics = self._topics
        mqtt = self._mqtt
        assert topics is not None and mqtt is not None
        mqtt.publish(topics.availability, b"online", retain=True)
        # Never present stale state as live: until the device answers a poll,
        # entities stay unavailable.
        mqtt.publish(
            topics.device_availability,
            b"online" if self._device_online else b"offline",
            retain=True,
        )
        if self._config.discovery_enabled:
            identity = self._last_identity
            assert identity is not None
            for topic, payload in build_discovery_configs(
                identity, topics, self._config.discovery_prefix
            ).items():
                mqtt.publish(topic, _dumps(payload), retain=True)
        for topic, payload in list(self._published.items()):
            mqtt.publish(topic, payload.encode("utf-8"), retain=True)

    def _on_mqtt_message(self, command_map: Dict[str, str], topic: str, payload: bytes) -> None:
        kind = command_map.get(topic)
        if kind is None:
            logger.warning("ignoring message on unsubscribed topic %r", topic)
            return
        command = parse_command(kind, payload, self._restore_level)
        if command is None:
            logger.warning(
                "rejected invalid %s command payload (%d bytes) — no device call, no mutation",
                kind,
                len(payload),
            )
            return
        logger.info("command %s %s %s", kind, command.path, _compact(command.body))
        try:
            self._device.command(command.path, command.body)
        except TypedDeviceError as exc:
            # §9 typed error: the device rejected without mutation; surface and move on.
            logger.warning("device rejected %s: %s", kind, exc)
            return
        except DeviceUnreachable as exc:
            logger.warning("device unreachable during %s: %s", kind, exc)
            self._mark_device_availability(False)
            return
        except DeviceProtocolError as exc:
            logger.warning("device protocol error during %s: %s", kind, exc)
            self._mark_device_availability(False)
            return
        now = time.monotonic()
        if now - self._last_poll_at >= _WAKE_MIN_GAP_S:
            self._wake.set()  # refresh state promptly after an accepted command

    # -- polling ----------------------------------------------------------------

    def _poll_loop(self, entity_set) -> None:
        interval = self._config.poll_interval_s
        while not self._stop.is_set():
            now = time.monotonic()
            due = now >= self._next_due
            if self._wake.is_set() and now - self._last_poll_at >= _WAKE_MIN_GAP_S:
                self._wake.clear()
                due = True
            elif self._wake.is_set():
                self._wake.clear()  # too soon after the last poll; the schedule covers it
            if due:
                self._poll_once(entity_set)
                self._last_poll_at = time.monotonic()
                self._next_due = self._last_poll_at + interval
            self._stop.wait(timeout=min(0.2, max(0.01, self._next_due - time.monotonic())))

    def _poll_once(self, entity_set) -> None:
        topics = self._topics
        mqtt = self._mqtt
        if topics is None or mqtt is None:
            return
        try:
            state = self._device.state()
        except DeviceUnreachable as exc:
            logger.info("device unreachable: %s", exc)
            self._mark_device_availability(False)
            return
        except DeviceError as exc:
            logger.warning("device poll failed: %s", exc)
            self._mark_device_availability(False)
            return
        self._last_state = state
        self._mark_device_availability(True)
        if state.brightness_current > 0:
            self._restore_level = state.brightness_current
        if not entity_set.mode_sensor:
            return
        payloads = build_state_payloads(state, self._restore_level)
        topic_map = state_topic_map(topics)
        for key, payload in payloads.items():
            topic = topic_map[key]
            if self._published.get(topic) == payload:
                continue
            mqtt.publish(topic, payload.encode("utf-8"), retain=True)
            self._published[topic] = payload

    def _mark_device_availability(self, online: bool) -> None:
        topics = self._topics
        mqtt = self._mqtt
        if topics is None or mqtt is None:
            self._device_online = online
            return
        if self._device_online == online:
            return
        self._device_online = online
        logger.info("device %s", "reachable" if online else "unreachable")
        mqtt.publish(
            topics.device_availability, b"online" if online else b"offline", retain=True
        )

    # -- startup helpers ----------------------------------------------------------

    def _wait_for_identity(self) -> DeviceIdentity:
        backoff = _BACKOFF_BASE_S
        attempt = 0
        while not self._stop.is_set():
            try:
                identity = self._device.identity()
                self._last_identity = identity
                return identity
            except DeviceUnreachable as exc:
                log = logger.info if attempt == 0 else logger.warning
                log("device identity unreachable (attempt %d): %s", attempt + 1, exc)
            except DeviceError as exc:
                logger.warning("device identity failed (attempt %d): %s", attempt + 1, exc)
            attempt += 1
            if self._stop.wait(timeout=min(backoff, _BACKOFF_MAX_S)):
                break
            backoff = min(backoff * 2, _BACKOFF_MAX_S)
        raise AdapterStopped("adapter stopped before the device became reachable")


def _dumps(payload: dict) -> str:
    return json.dumps(payload, separators=(",", ":"), ensure_ascii=True)


def _compact(body: dict) -> str:
    return json.dumps(body, separators=(",", ":"))
