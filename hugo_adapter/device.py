"""LED Wall semantic API client (host side, HUGO DeviceAdapter).

COPY-ADAPT of the accepted LED-L005 ``ha_adapter/device.py`` (in-repo
heritage), kept independent so the HUGO and Home Assistant adapters never
couple. Speaks exactly the endpoints documented in docs/api/CONTRACT.md (the
accepted LED-L003 wire contract): GET /identity, GET /state, and the §5
command POSTs. Responses are classified deterministically:

- success (HTTP 200)
- :class:`TypedDeviceError` — the device's §9 typed error envelope
- :class:`DeviceProtocolError` — anything malformed (bad JSON, wrong shape,
  oversized body, unexpected redirect)
- :class:`DeviceUnreachable` — connection/timeout failures

Adaptation vs the LED-L005 source: identity normalization additionally keeps
``role``/``protocolVersion``/``matrix`` (public per §10) because the HUGO tool
surface serves them to the AI client; state normalization is unchanged.

State payloads are normalized fail-safe: missing or wrongly-typed fields fall
back to neutral defaults instead of raising, so one bad field can never wedge
the adapter (bounded caps everywhere).
"""

from __future__ import annotations

import json
import socket
import urllib.error
import urllib.request
from dataclasses import dataclass
from typing import Any, Dict, List, Optional, Tuple

# Bounded response size: device state payloads are ~1-2 KB; 64 KB is a
# generous ceiling that still bounds memory against a rogue peer.
_MAX_BODY_BYTES = 64 * 1024

_TEXT_CAP = 128
_NAME_CAP = 64
_ID_CAP = 64
_WARN_CAP = 16
_WARN_LEN_CAP = 200
_CAPS_CAP = 16


class DeviceError(Exception):
    """Base class for device client failures."""


class TypedDeviceError(DeviceError):
    """The device answered with the §9 typed error envelope."""

    def __init__(self, code: str, message: str, available: Optional[str] = None):
        super().__init__(f"{code}: {message}" + (f" (available: {available})" if available else ""))
        self.code = code
        self.message = message
        self.available = available


class DeviceProtocolError(DeviceError):
    """The device answered with something the contract does not allow."""


class DeviceUnreachable(DeviceError):
    """Connection or timeout failure — the device cannot be reached."""


@dataclass(frozen=True)
class DeviceIdentity:
    device_id: str
    device_type: str
    role: str
    protocol_version: int
    fw_version: str
    capabilities: Tuple[str, ...]
    matrix_width: int
    matrix_height: int


@dataclass(frozen=True)
class DeviceState:
    available: bool
    device_id: str
    base_mode: str
    current_mode: str
    active_scene: Optional[str]
    active_animation: Optional[str]
    active_expression: Optional[str]
    transient_kind: Optional[str]
    transient_remaining_ms: Optional[int]
    text_current: Optional[str]
    text_scrolling: bool
    brightness_current: int
    brightness_max: int
    uptime_seconds: int
    warnings: Tuple[str, ...]


def _norm_str(value: Any, cap: int) -> Optional[str]:
    if not isinstance(value, str):
        return None
    return value[:cap]


def _norm_int(value: Any, default: int = 0) -> int:
    # bool is an int subclass; reject it explicitly.
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        return default
    if isinstance(value, float) and not value.is_integer():
        return default
    return int(value)


def _norm_bool(value: Any, default: bool = False) -> bool:
    if not isinstance(value, bool):
        return default
    return value


def _norm_name_map(value: Any) -> Optional[str]:
    """activeScene/activeAnimation/activeExpression: a name or JSON null."""
    return _norm_str(value, _NAME_CAP) if value is not None else None


def _norm_transient(value: Any) -> Tuple[Optional[str], Optional[int]]:
    if not isinstance(value, dict):
        return None, None
    kind = _norm_str(value.get("kind"), _NAME_CAP)
    remaining = value.get("remainingMs")
    if kind is None or isinstance(remaining, bool) or not isinstance(remaining, (int, float)):
        return None, None
    if isinstance(remaining, float) and not remaining.is_integer():
        return None, None
    return kind, max(0, int(remaining))


def normalize_state(raw: Any) -> DeviceState:
    """Fail-safe normalization of a GET /state payload into DeviceState."""
    if not isinstance(raw, dict):
        raw = {}
    text_raw = raw.get("text")
    text_current: Optional[str] = None
    text_scrolling = False
    if isinstance(text_raw, dict):
        text_current = _norm_str(text_raw.get("current"), _TEXT_CAP)
        text_scrolling = _norm_bool(text_raw.get("scrolling"))
    warnings_raw = raw.get("warnings")
    warnings: List[str] = []
    if isinstance(warnings_raw, list):
        for item in warnings_raw[:_WARN_CAP]:
            if isinstance(item, str):
                warnings.append(item[:_WARN_LEN_CAP])
    transient_kind, transient_remaining = _norm_transient(raw.get("activeTransient"))
    brightness_raw = raw.get("brightness")
    if isinstance(brightness_raw, dict):
        brightness_current = max(0, _norm_int(brightness_raw.get("current")))
        brightness_max = max(0, _norm_int(brightness_raw.get("max")))
    else:
        brightness_current, brightness_max = 0, 0
    return DeviceState(
        available=_norm_bool(raw.get("available"), True),
        device_id=_norm_str(raw.get("deviceId"), _ID_CAP) or "led-wall",
        base_mode=_norm_str(raw.get("baseMode"), _NAME_CAP) or "idle",
        current_mode=_norm_str(raw.get("currentMode"), _NAME_CAP) or "idle",
        active_scene=_norm_name_map(raw.get("activeScene")),
        active_animation=_norm_name_map(raw.get("activeAnimation")),
        active_expression=_norm_name_map(raw.get("activeExpression")),
        transient_kind=transient_kind,
        transient_remaining_ms=transient_remaining,
        text_current=text_current,
        text_scrolling=text_scrolling,
        brightness_current=brightness_current,
        brightness_max=brightness_max,
        uptime_seconds=max(0, _norm_int(raw.get("uptimeSeconds"))),
        warnings=tuple(warnings),
    )


def normalize_identity(raw: Any) -> DeviceIdentity:
    if not isinstance(raw, dict):
        raw = {}
    caps_raw = raw.get("capabilities")
    caps: List[str] = []
    if isinstance(caps_raw, list):
        for item in caps_raw[:_CAPS_CAP]:
            if isinstance(item, str) and item and item not in caps:
                caps.append(item[:_ID_CAP])
    matrix_raw = raw.get("matrix")
    matrix_width, matrix_height = 0, 0
    if isinstance(matrix_raw, dict):
        matrix_width = max(0, min(1024, _norm_int(matrix_raw.get("width"))))
        matrix_height = max(0, min(1024, _norm_int(matrix_raw.get("height"))))
    return DeviceIdentity(
        device_id=_norm_str(raw.get("deviceId"), _ID_CAP) or "led-wall",
        device_type=_norm_str(raw.get("deviceType"), _NAME_CAP) or "visual_display_v1",
        role=_norm_str(raw.get("role"), _NAME_CAP) or "display",
        protocol_version=max(0, _norm_int(raw.get("protocolVersion"), 1)),
        fw_version=_norm_str(raw.get("fwVersion"), _NAME_CAP) or "unknown",
        capabilities=tuple(caps),
        matrix_width=matrix_width,
        matrix_height=matrix_height,
    )


class _NoRedirect(urllib.request.HTTPRedirectHandler):
    """The device API never redirects; treat any 3xx as a protocol error."""

    def redirect_request(self, req, fp, code, msg, headers, newurl):  # noqa: D102 (stdlib hook)
        return None


class DeviceClient:
    """Bounded HTTP client for the LED Wall semantic API."""

    def __init__(self, base_url: str, timeout_s: float = 3.0):
        if not base_url.startswith("http://"):
            raise ValueError("device_url must be an http:// URL in v1")
        # No credentials in the URL: the API has no v1 authentication, and
        # userinfo would leak into exception messages surfaced to the client.
        if "@" in base_url[len("http://"):].split("/", 1)[0]:
            raise ValueError("device_url must not contain credentials (user:pass@)")
        self._base = base_url.rstrip("/")
        self._timeout_s = timeout_s
        self._opener = urllib.request.build_opener(_NoRedirect)

    # -- raw request core ---------------------------------------------------

    def _request(self, path: str, payload: Optional[Dict[str, Any]]) -> Any:
        data = None
        headers = {"Accept": "application/json"}
        if payload is not None:
            data = json.dumps(payload, separators=(",", ":")).encode("utf-8")
            headers["Content-Type"] = "application/json"
        req = urllib.request.Request(self._base + path, data=data, headers=headers)
        try:
            with self._opener.open(req, timeout=self._timeout_s) as resp:
                body = resp.read(_MAX_BODY_BYTES + 1)
        except DeviceError:
            raise
        except (urllib.error.URLError, urllib.error.HTTPError, socket.timeout, OSError) as exc:
            # HTTPError is a URLError subclass carrying a 3xx/4xx/5xx status;
            # connection-level failures surface as plain URLError/OSError.
            if isinstance(exc, urllib.error.HTTPError):
                try:
                    self._raise_classified(int(exc.code), exc.read(_MAX_BODY_BYTES + 1))
                finally:
                    exc.close()
            raise DeviceUnreachable(f"{type(exc).__name__}: {exc}") from exc
        if len(body) > _MAX_BODY_BYTES:
            raise DeviceProtocolError("response body exceeds 64 KiB cap")
        try:
            return json.loads(body.decode("utf-8"))
        except (json.JSONDecodeError, UnicodeDecodeError) as exc:
            raise DeviceProtocolError(f"response is not valid JSON: {exc}") from exc

    @staticmethod
    def _raise_classified(status: int, body: bytes) -> None:
        try:
            envelope = json.loads(body.decode("utf-8"))
        except (json.JSONDecodeError, UnicodeDecodeError, ValueError):
            raise DeviceProtocolError(f"HTTP {status} without a typed error envelope") from None
        error = envelope.get("error") if isinstance(envelope, dict) else None
        if (
            isinstance(envelope, dict)
            and envelope.get("ok") is False
            and isinstance(error, dict)
            and isinstance(error.get("code"), str)
        ):
            available = error.get("available")
            raise TypedDeviceError(
                error["code"],
                error.get("message") if isinstance(error.get("message"), str) else "",
                available if isinstance(available, str) else None,
            )
        raise DeviceProtocolError(f"HTTP {status} without a typed error envelope")

    # -- public API (CONTRACT.md surface) ------------------------------------

    def identity(self) -> DeviceIdentity:
        return normalize_identity(self._request("/identity", None))

    def state(self) -> DeviceState:
        return normalize_state(self._request("/state", None))

    def command(self, path: str, payload: Dict[str, Any]) -> None:
        """POST a §5 command. Raises TypedDeviceError on the device's typed error."""
        result = self._request(path, payload)
        if isinstance(result, dict) and result.get("ok") is False:
            # 200 with an error envelope is not in the contract; classify it anyway.
            error = result.get("error")
            if isinstance(error, dict) and isinstance(error.get("code"), str):
                raise TypedDeviceError(error["code"], str(error.get("message", "")))
            raise DeviceProtocolError("200 response with ok=false and no typed error")
