"""DeviceAdapter service: capability cache + tool dispatch + stdio loop.

Deterministic device policy lives here; the AI client stays above it. The
service owns no visual state (the device is authoritative), keeps no sessions,
and runs a sequential single-threaded loop: one request line in, at most one
response line out, notifications never answered (JSON-RPC 2.0 §4).

Capability eligibility is TTL-bounded and fails closed (frozen spec §10:
discovery never implies authorization): core §5 capabilities are always
advertised by the device, so core tools are always eligible; conditional
§16 tools (color/spectrum) require a fresh, successful identity read.
"""

from __future__ import annotations

import sys
import time
from typing import Any, BinaryIO, Callable, Dict, Optional, Tuple

from . import intents, rpc
from .config import AdapterConfig
from .device import DeviceClient, DeviceIdentity, DeviceProtocolError, DeviceState, DeviceUnreachable, TypedDeviceError
from .intents import InvalidArguments, ToolSpec

# Maximum accepted request line length (bytes, UTF-8) — bounds memory and
# parse cost; tool payloads are far smaller (text cap 128 chars).
MAX_LINE_BYTES = 64 * 1024


def state_summary(state: DeviceState) -> Dict[str, Any]:
    """Bounded §4 state summary for the get_state tool result."""
    return {
        "available": state.available,
        "deviceId": state.device_id,
        "baseMode": state.base_mode,
        "currentMode": state.current_mode,
        "activeScene": state.active_scene,
        "activeAnimation": state.active_animation,
        "activeExpression": state.active_expression,
        "activeTransient": (
            {"kind": state.transient_kind, "remainingMs": state.transient_remaining_ms}
            if state.transient_kind is not None
            else None
        ),
        "text": (
            {"current": state.text_current, "scrolling": state.text_scrolling}
            if state.text_current is not None
            else None
        ),
        "brightness": {"current": state.brightness_current, "max": state.brightness_max},
        "uptimeSeconds": state.uptime_seconds,
        "warnings": list(state.warnings),
    }


def identity_summary(identity: DeviceIdentity) -> Dict[str, Any]:
    """Bounded §10 identity summary for the get_device_info tool result."""
    return {
        "deviceId": identity.device_id,
        "deviceType": identity.device_type,
        "role": identity.role,
        "protocolVersion": identity.protocol_version,
        "fwVersion": identity.fw_version,
        "capabilities": list(identity.capabilities),
        "matrix": {"width": identity.matrix_width, "height": identity.matrix_height},
    }


class CapabilityCache:
    """TTL-bounded GET /identity cache; refresh failure fails closed."""

    def __init__(
        self,
        client: DeviceClient,
        ttl_s: int,
        clock: Callable[[], float] = time.monotonic,
        log: Callable[[str], None] = lambda msg: None,
    ):
        self._client = client
        self._ttl_s = ttl_s
        self._clock = clock
        self._log = log
        self._identity: Optional[DeviceIdentity] = None
        self._last_success: float = float("-inf")
        self._last_attempt: float = float("-inf")

    def _refresh_if_due(self) -> None:
        now = self._clock()
        if self._identity is not None and (now - self._last_success) <= self._ttl_s:
            return
        if (now - self._last_attempt) < self._ttl_s:
            return  # rate-limit attempts against a dead device (one per TTL)
        self._last_attempt = now
        try:
            self._identity = self._client.identity()
            self._last_success = now
        except Exception as exc:  # DeviceUnreachable/TypedDeviceError/DeviceProtocolError
            self._identity = None  # fail closed: conditional tools lose eligibility
            self._log(f"capability refresh failed ({type(exc).__name__}: {exc}); conditional tools unavailable")

    def capabilities(self) -> Optional[Tuple[str, ...]]:
        self._refresh_if_due()
        if self._identity is None:
            return None
        return self._identity.capabilities

    def has(self, capability: str) -> bool:
        caps = self.capabilities()
        return caps is not None and capability in caps


class DeviceAdapterService:
    """The HUGO-facing stdio JSON-RPC tool surface over the LED Wall API."""

    def __init__(
        self,
        client: DeviceClient,
        config: AdapterConfig,
        clock: Callable[[], float] = time.monotonic,
        log: Callable[[str], None] = lambda msg: None,
    ):
        self._client = client
        self._config = config
        self._caps = CapabilityCache(client, config.capability_ttl_s, clock=clock, log=log)
        self._log = log

    # -- request handling ----------------------------------------------------

    def handle_line(self, line: str) -> Optional[str]:
        """Handle one decoded request line; return the response line or None.

        Notifications (no "id") are dispatched but never answered (§4).
        """
        if not line.strip():
            return None  # blank framing noise — skipped, never answered
        try:
            is_notification, request_id, method, params = rpc.parse_request(line)
        except rpc.RpcError as exc:
            return rpc.error_response(None, exc.code, exc.message)
        try:
            response = self._dispatch(request_id, method, params)
        except rpc.RpcError as exc:
            response = rpc.error_response(request_id, exc.code, exc.message, exc.data)
        except Exception as exc:  # never leak a traceback onto the protocol
            self._log(f"internal error in {method}: {type(exc).__name__}: {exc}")
            response = rpc.error_response(request_id, rpc.INTERNAL_ERROR, "Internal error")
        if is_notification:
            return None
        return response

    def _dispatch(self, request_id: Any, method: str, params: Any) -> Optional[str]:
        if method == "ping":
            self._expect_object_params(params)
            return rpc.result_response(request_id, {"pong": True, "service": "ledwall-hugo-deviceadapter"})
        if method == "tools/list":
            self._expect_object_params(params)
            return self._tools_list(request_id)
        if method == "tools/call":
            return self._tools_call(request_id, params)
        raise rpc.RpcError(rpc.METHOD_NOT_FOUND, "Method not found", {"kind": "unknownMethod", "method": method[:64]})

    @staticmethod
    def _expect_object_params(params: Any) -> Dict[str, Any]:
        if params is None:
            return {}
        if not isinstance(params, dict):
            raise rpc.RpcError(rpc.INVALID_PARAMS, "Invalid params", {"kind": "invalidArguments"})
        return params

    def _tools_list(self, request_id: Any) -> str:
        caps = self._caps.capabilities()
        tools = []
        for spec in intents.tool_listing(caps):
            entry: Dict[str, Any] = {"name": spec.name, "description": spec.summary}
            if spec.capability is not None:
                entry["requiresCapability"] = spec.capability
            tools.append(entry)
        return rpc.result_response(request_id, {"tools": tools})

    def _tools_call(self, request_id: Any, params: Any) -> str:
        params = self._expect_object_params(params)
        unknown = [key for key in params if key not in ("name", "arguments")]
        if unknown:
            raise rpc.RpcError(
                rpc.INVALID_PARAMS, "Invalid params", {"kind": "invalidArguments", "detail": f"unknown parameter: {unknown[0]}"}
            )
        name = params.get("name")
        if not isinstance(name, str) or not name:
            raise rpc.RpcError(rpc.INVALID_PARAMS, "Invalid params", {"kind": "invalidArguments", "detail": "'name' must be a non-empty string"})
        spec = intents.tool_by_name(name)
        if spec is None:
            raise rpc.RpcError(
                rpc.INVALID_PARAMS,
                f"Unknown tool: {name[:64]}",
                {"kind": "unknownTool", "tool": name[:64]},
            )
        if spec.capability is not None and not self._caps.has(spec.capability):
            raise rpc.RpcError(
                rpc.DEVICE_ERROR,
                f"Tool not available: {spec.name} (capability not advertised)",
                {"kind": "unsupportedCapability", "capability": spec.capability},
            )
        arguments = params.get("arguments")
        try:
            http_method, path, payload = intents.build_request(spec, arguments)
        except InvalidArguments as exc:
            raise rpc.RpcError(
                rpc.INVALID_PARAMS,
                f"Invalid arguments for {spec.name}: {exc}",
                {"kind": "invalidArguments", "tool": spec.name},
            ) from None
        return self._execute(request_id, spec, http_method, path, payload)

    def _execute(self, request_id: Any, spec: ToolSpec, http_method: str, path: str, payload: Dict[str, Any]) -> str:
        try:
            if http_method == "GET":
                if path == "/state":
                    result: Dict[str, Any] = {"ok": True, "tool": spec.name, "state": state_summary(self._client.state())}
                else:
                    result = {"ok": True, "tool": spec.name, "identity": identity_summary(self._client.identity())}
            else:
                self._client.command(path, payload)
                result = {"ok": True, "tool": spec.name, "executed": True}
        except TypedDeviceError as exc:
            data: Dict[str, Any] = {"kind": "deviceError", "code": exc.code, "message": exc.message}
            if exc.available is not None:
                data["available"] = exc.available
            raise rpc.RpcError(rpc.DEVICE_ERROR, f"Device rejected {spec.name}: {exc.code}", data) from None
        except DeviceUnreachable as exc:
            raise rpc.RpcError(
                rpc.DEVICE_ERROR,
                f"Device unreachable for {spec.name}",
                {"kind": "deviceUnreachable", "detail": str(exc)[:200]},
            ) from None
        except DeviceProtocolError as exc:
            raise rpc.RpcError(
                rpc.DEVICE_ERROR,
                f"Device protocol error for {spec.name}",
                {"kind": "deviceProtocol", "detail": str(exc)[:200]},
            ) from None
        return rpc.result_response(request_id, result)

    # -- stdio loop -----------------------------------------------------------

    def run(self, stdin: BinaryIO = None, stdout: BinaryIO = None) -> None:
        """Serve line-delimited JSON-RPC until stdin EOF.

        Responses go only to stdout (protocol purity); the caller routes logs
        to stderr. Oversized lines are consumed to resync and answered with a
        single -32600 error.
        """
        if stdin is None:
            stdin = sys.stdin.buffer
        if stdout is None:
            stdout = sys.stdout.buffer
        while True:
            line = stdin.readline(MAX_LINE_BYTES + 1)
            if not line:
                break
            oversized = len(line) > MAX_LINE_BYTES
            if oversized:
                while not line.endswith(b"\n"):  # drain the rest of the line
                    rest = stdin.readline(MAX_LINE_BYTES + 1)
                    if not rest:
                        break
                    line = rest
                response = rpc.error_response(
                    None, rpc.INVALID_REQUEST, f"Request line exceeds {MAX_LINE_BYTES} bytes"
                )
            else:
                try:
                    decoded = line.decode("utf-8")
                except UnicodeDecodeError:
                    response = rpc.parse_error_response()
                else:
                    response = self.handle_line(decoded)
            if response is not None:
                stdout.write(rpc.encode_response(response))
                stdout.flush()
