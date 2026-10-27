"""HUGO DeviceAdapter JSON-RPC codec tests (C-L006-1): framing, id
correlation, notifications, error codes, deterministic byte shapes.

Error codes are independently derived from the JSON-RPC 2.0 specification
(§5.1 reserved codes; §4 notifications).
"""

import json
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from hugo_adapter import rpc  # noqa: E402


def request(**overrides):
    base = {"jsonrpc": "2.0", "method": "ping", "id": 1}
    base.update(overrides)
    return json.dumps(base)


class ParseRequestTest(unittest.TestCase):
    def test_valid_request_int_id(self):
        is_notif, request_id, method, params = rpc.parse_request(request())
        self.assertFalse(is_notif)
        self.assertEqual(request_id, 1)
        self.assertEqual(method, "ping")
        self.assertIsNone(params)

    def test_valid_request_string_id(self):
        _, request_id, _, _ = rpc.parse_request(request(id="abc-123"))
        self.assertEqual(request_id, "abc-123")

    def test_id_echo_boundary_128_chars(self):
        long_id = "x" * 128
        _, request_id, _, _ = rpc.parse_request(request(id=long_id))
        self.assertEqual(request_id, long_id)
        with self.assertRaises(rpc.RpcError) as ctx:
            rpc.parse_request(request(id="x" * 129))
        self.assertEqual(ctx.exception.code, rpc.INVALID_REQUEST)

    def test_null_id_is_a_request_not_a_notification(self):
        # JSON-RPC 2.0: an explicit null id is a request and MUST be answered
        # with id null; only a *missing* id makes a notification.
        is_notif, request_id, _, _ = rpc.parse_request(request(id=None))
        self.assertFalse(is_notif)
        self.assertIsNone(request_id)

    def test_bool_id_rejected(self):
        with self.assertRaises(rpc.RpcError) as ctx:
            rpc.parse_request(request(id=True))
        self.assertEqual(ctx.exception.code, rpc.INVALID_REQUEST)

    def test_float_id_rejected(self):
        with self.assertRaises(rpc.RpcError) as ctx:
            rpc.parse_request(request(id=1.5))
        self.assertEqual(ctx.exception.code, rpc.INVALID_REQUEST)

    def test_missing_id_is_notification(self):
        is_notif, request_id, _, _ = rpc.parse_request(json.dumps({"jsonrpc": "2.0", "method": "ping"}))
        self.assertTrue(is_notif)
        self.assertIsNone(request_id)

    def test_wrong_jsonrpc_version_rejected(self):
        for bad in ("1.0", 2.0, None):
            with self.assertRaises(rpc.RpcError) as ctx:
                rpc.parse_request(request(jsonrpc=bad))
            self.assertEqual(ctx.exception.code, rpc.INVALID_REQUEST)

    def test_missing_jsonrpc_rejected(self):
        with self.assertRaises(rpc.RpcError) as ctx:
            rpc.parse_request(json.dumps({"method": "ping", "id": 1}))
        self.assertEqual(ctx.exception.code, rpc.INVALID_REQUEST)

    def test_missing_method_rejected(self):
        with self.assertRaises(rpc.RpcError) as ctx:
            rpc.parse_request(json.dumps({"jsonrpc": "2.0", "id": 1}))
        self.assertEqual(ctx.exception.code, rpc.INVALID_REQUEST)

    def test_non_string_method_rejected(self):
        with self.assertRaises(rpc.RpcError) as ctx:
            rpc.parse_request(request(method=7))
        self.assertEqual(ctx.exception.code, rpc.INVALID_REQUEST)

    def test_object_params_accepted(self):
        _, _, _, params = rpc.parse_request(request(params={"a": 1}))
        self.assertEqual(params, {"a": 1})

    def test_array_params_structurally_accepted(self):
        # Positional params are structurally valid JSON-RPC; the service
        # rejects them with -32602 (named-params-only methods).
        _, _, _, params = rpc.parse_request(request(params=[1, 2]))
        self.assertEqual(params, [1, 2])

    def test_string_params_rejected(self):
        with self.assertRaises(rpc.RpcError) as ctx:
            rpc.parse_request(request(params="oops"))
        self.assertEqual(ctx.exception.code, rpc.INVALID_REQUEST)

    def test_batch_array_rejected_as_invalid_request(self):
        # Documented limitation: no batch support; the spec allows answering
        # an unrecognizable array with a single error object.
        with self.assertRaises(rpc.RpcError) as ctx:
            rpc.parse_request(json.dumps([request(), request()]))
        self.assertEqual(ctx.exception.code, rpc.INVALID_REQUEST)

    def test_non_object_root_rejected(self):
        for bad in ('"str"', "42", "true", "null"):
            with self.assertRaises(rpc.RpcError) as ctx:
                rpc.parse_request(bad)
            self.assertEqual(ctx.exception.code, rpc.INVALID_REQUEST)

    def test_invalid_json_is_parse_error(self):
        for bad in ("{", "{,}", "]]"):
            with self.assertRaises(rpc.RpcError) as ctx:
                rpc.parse_request(bad)
            self.assertEqual(ctx.exception.code, rpc.PARSE_ERROR)


class ResponseShapeTest(unittest.TestCase):
    def test_result_response_shape_and_determinism(self):
        line = rpc.result_response(7, {"ok": True, "tool": "ping"})
        self.assertEqual(line, '{"id":7,"jsonrpc":"2.0","result":{"ok":true,"tool":"ping"}}')
        self.assertEqual(rpc.result_response(7, {"ok": True, "tool": "ping"}), line)

    def test_error_response_includes_data(self):
        line = rpc.error_response("req-1", rpc.DEVICE_ERROR, "boom", {"kind": "deviceUnreachable"})
        parsed = json.loads(line)
        self.assertEqual(parsed["id"], "req-1")
        self.assertEqual(parsed["error"]["code"], -32000)
        self.assertEqual(parsed["error"]["message"], "boom")
        self.assertEqual(parsed["error"]["data"], {"kind": "deviceUnreachable"})

    def test_error_response_omits_data_when_absent(self):
        parsed = json.loads(rpc.error_response(3, rpc.METHOD_NOT_FOUND, "Method not found"))
        self.assertEqual(parsed["error"]["code"], -32601)
        self.assertNotIn("data", parsed["error"])

    def test_parse_error_response_has_null_id(self):
        parsed = json.loads(rpc.parse_error_response())
        self.assertIsNone(parsed["id"])
        self.assertEqual(parsed["error"]["code"], -32700)

    def test_encode_response_appends_newline(self):
        self.assertEqual(rpc.encode_response('{"id":1}'), b'{"id":1}\n')

    def test_unicode_survives_round_trip(self):
        line = rpc.result_response("u1", {"text": "Fünf → düster"})
        self.assertIn("Fünf → düster", line)
        parsed = json.loads(line)
        self.assertEqual(parsed["result"]["text"], "Fünf → düster")


class ReservedCodesTest(unittest.TestCase):
    def test_reserved_code_values_match_jsonrpc_spec(self):
        # Independently transcribed from JSON-RPC 2.0 §5.1.
        self.assertEqual(rpc.PARSE_ERROR, -32700)
        self.assertEqual(rpc.INVALID_REQUEST, -32600)
        self.assertEqual(rpc.METHOD_NOT_FOUND, -32601)
        self.assertEqual(rpc.INVALID_PARAMS, -32602)
        self.assertEqual(rpc.INTERNAL_ERROR, -32603)

    def test_server_error_in_implementation_range(self):
        self.assertEqual(rpc.DEVICE_ERROR, -32000)  # within -32000..-32099


if __name__ == "__main__":
    unittest.main()
