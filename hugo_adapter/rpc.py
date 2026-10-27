"""JSON-RPC 2.0 codec for the DeviceAdapter stdio surface (LED-L006).

Line-delimited UTF-8 JSON on stdin/stdout: one Request object per line, one
Response object per line back. Batches are rejected with a single -32600
error object (documented limitation — the tool surface has no batched use).
Notifications (requests without "id") are executed by the service but never
answered, per JSON-RPC 2.0 §4.

The codec is pure (no I/O) so the suites can pin every error shape
deterministically. All server errors carry a bounded ``data`` object with a
``kind`` discriminator plus the §9 typed device fields when applicable.
"""

from __future__ import annotations

import json
from typing import Any, Dict, Optional, Tuple

# JSON-RPC 2.0 reserved error codes (§5.1).
PARSE_ERROR = -32700
INVALID_REQUEST = -32600
METHOD_NOT_FOUND = -32601
INVALID_PARAMS = -32602
INTERNAL_ERROR = -32603
# Server-defined (§5.1 reserved for implementation-defined server errors).
DEVICE_ERROR = -32000

# Maximum accepted id length (string form); ints already bounded by JSON.
MAX_ID_CHARS = 128

_KNOWN_METHODS = ("ping", "tools/list", "tools/call")


class RpcError(Exception):
    """A request-level JSON-RPC failure with its error code and data."""

    def __init__(self, code: int, message: str, data: Optional[Dict[str, Any]] = None):
        super().__init__(message)
        self.code = code
        self.message = message
        self.data = data


def _encode(obj: Any) -> str:
    return json.dumps(obj, separators=(",", ":"), sort_keys=True, ensure_ascii=False)


def result_response(request_id: Any, result: Dict[str, Any]) -> str:
    return _encode({"jsonrpc": "2.0", "id": request_id, "result": result})


def error_response(
    request_id: Any,
    code: int,
    message: str,
    data: Optional[Dict[str, Any]] = None,
) -> str:
    error: Dict[str, Any] = {"code": code, "message": message}
    if data is not None:
        error["data"] = data
    return _encode({"jsonrpc": "2.0", "id": request_id, "error": error})


def parse_error_response() -> str:
    return error_response(None, PARSE_ERROR, "Parse error")


def parse_request(line: str) -> Tuple[bool, Any, str, Any]:
    """Parse one non-blank request line into (is_notification, id, method, params).

    A notification (no "id" member) is returned with ``is_notification=True``
    and id None: the caller SHOULD execute it and MUST NOT answer it
    (JSON-RPC 2.0 §4). A request with an explicit JSON null id is NOT a
    notification; it is answered with id null. Raises
    RpcError(PARSE_ERROR/INVALID_REQUEST) for anything malformed. The caller
    skips blank lines before calling (framing noise).
    """
    try:
        request = json.loads(line)
    except (json.JSONDecodeError, ValueError):
        raise RpcError(PARSE_ERROR, "Parse error") from None
    if not isinstance(request, dict):
        raise RpcError(INVALID_REQUEST, "Invalid Request")
    if request.get("jsonrpc") != "2.0":
        raise RpcError(INVALID_REQUEST, "Invalid Request")
    method = request.get("method")
    if not isinstance(method, str) or not method:
        raise RpcError(INVALID_REQUEST, "Invalid Request")
    params = request.get("params")
    if params is not None and not isinstance(params, (dict, list)):
        raise RpcError(INVALID_REQUEST, "Invalid Request")
    if "id" not in request:
        return (True, None, method, params)
    request_id = request["id"]
    if isinstance(request_id, bool) or isinstance(request_id, float):
        raise RpcError(INVALID_REQUEST, "Invalid Request")
    if isinstance(request_id, str):
        if len(request_id) > MAX_ID_CHARS:
            raise RpcError(INVALID_REQUEST, "Invalid Request")
    elif not isinstance(request_id, int) and request_id is not None:
        raise RpcError(INVALID_REQUEST, "Invalid Request")
    return (False, request_id, method, params)


def encode_response(response: str) -> bytes:
    """Frame one response line for stdout."""
    return response.encode("utf-8") + b"\n"
