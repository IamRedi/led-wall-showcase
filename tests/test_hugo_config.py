"""HUGO DeviceAdapter config tests (C-L006-1/C-L006-9): validation, safe
fallbacks, fail-closed behavior, hygiene."""

import json
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from hugo_adapter.config import ConfigError, load_config  # noqa: E402

VALID = {
    "device_url": "http://led-wall.local/",
}


def write_config(data) -> str:
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as fh:
        if isinstance(data, str):
            fh.write(data)
        else:
            json.dump(data, fh)
        return fh.name


class LoadConfigTest(unittest.TestCase):
    def test_minimal_config_applies_defaults(self):
        config = load_config(write_config(VALID))
        self.assertEqual(config.device_url, "http://led-wall.local")  # trailing slash normalized
        self.assertEqual(config.device_timeout_s, 3.0)
        self.assertEqual(config.capability_ttl_s, 5)
        self.assertEqual(config.log_level, "INFO")
        self.assertEqual(config.warnings, ())

    def test_explicit_values_win(self):
        config = load_config(
            write_config(
                {
                    "device_url": "http://192.168.1.50",
                    "device_timeout_s": 1.5,
                    "capability_ttl_s": 30,
                    "log_level": "warning",
                }
            )
        )
        self.assertEqual(config.device_url, "http://192.168.1.50")
        self.assertEqual(config.device_timeout_s, 1.5)
        self.assertEqual(config.capability_ttl_s, 30)
        self.assertEqual(config.log_level, "WARNING")

    def test_integer_timeout_accepted_as_float(self):
        config = load_config(write_config({**VALID, "device_timeout_s": 5}))
        self.assertEqual(config.device_timeout_s, 5.0)
        self.assertIsInstance(config.device_timeout_s, float)

    def test_unknown_keys_ignored_with_warning(self):
        config = load_config(write_config({**VALID, "future_key": 1, "another": "x"}))
        self.assertEqual(
            config.warnings,
            ("unknown config key ignored: another", "unknown config key ignored: future_key"),
        )

    def test_ttl_clamped_low_with_warning(self):
        config = load_config(write_config({**VALID, "capability_ttl_s": 0}))
        self.assertEqual(config.capability_ttl_s, 1)
        self.assertEqual(config.warnings, ("config field 'capability_ttl_s' clamped 0 -> 1",))

    def test_ttl_clamped_high_with_warning(self):
        config = load_config(write_config({**VALID, "capability_ttl_s": 301}))
        self.assertEqual(config.capability_ttl_s, 300)
        self.assertEqual(config.warnings, ("config field 'capability_ttl_s' clamped 301 -> 300",))

    def test_ttl_boundaries_accepted_without_warning(self):
        for ttl in (1, 300):
            config = load_config(write_config({**VALID, "capability_ttl_s": ttl}))
            self.assertEqual(config.capability_ttl_s, ttl)
            self.assertEqual(config.warnings, ())


class FailClosedTest(unittest.TestCase):
    def test_missing_file_fails_closed(self):
        with self.assertRaises(ConfigError):
            load_config("/nonexistent/hugo-adapter-config.json")

    def test_invalid_json_fails_closed(self):
        with self.assertRaises(ConfigError):
            load_config(write_config("{not json"))

    def test_non_object_fails_closed(self):
        with self.assertRaises(ConfigError):
            load_config(write_config([VALID]))

    def test_missing_device_url_fails_closed(self):
        with self.assertRaises(ConfigError):
            load_config(write_config({"device_timeout_s": 3}))

    def test_empty_device_url_fails_closed(self):
        with self.assertRaises(ConfigError):
            load_config(write_config({**VALID, "device_url": "   "}))

    def test_non_string_device_url_fails_closed(self):
        with self.assertRaises(ConfigError):
            load_config(write_config({**VALID, "device_url": 42}))

    def test_https_url_rejected(self):
        with self.assertRaises(ConfigError):
            load_config(write_config({**VALID, "device_url": "https://led-wall.local"}))

    def test_ftp_url_rejected(self):
        with self.assertRaises(ConfigError):
            load_config(write_config({**VALID, "device_url": "ftp://led-wall.local"}))

    def test_overlong_device_url_rejected(self):
        with self.assertRaises(ConfigError):
            load_config(write_config({**VALID, "device_url": "http://" + "a" * 300}))

    def test_userinfo_in_device_url_rejected(self):
        # Adversarial: credentials in the URL would leak into exception
        # messages surfaced to the AI client; the API has no v1 auth anyway.
        with self.assertRaises(ConfigError):
            load_config(write_config({**VALID, "device_url": "http://user:pass@led-wall.local"}))
        with self.assertRaises(ConfigError):
            load_config(write_config({**VALID, "device_url": "http://user@led-wall.local"}))

    def test_userinfo_detection_scopes_to_authority(self):
        # An "@" in the path/query is not credentials and must be accepted.
        config = load_config(write_config({**VALID, "device_url": "http://led-wall.local/x@y"}))
        self.assertEqual(config.device_url, "http://led-wall.local/x@y")

    def test_timeout_zero_rejected(self):
        with self.assertRaises(ConfigError):
            load_config(write_config({**VALID, "device_timeout_s": 0}))

    def test_timeout_negative_rejected(self):
        with self.assertRaises(ConfigError):
            load_config(write_config({**VALID, "device_timeout_s": -1}))

    def test_timeout_above_cap_rejected(self):
        with self.assertRaises(ConfigError):
            load_config(write_config({**VALID, "device_timeout_s": 30.1}))

    def test_timeout_bool_rejected(self):
        with self.assertRaises(ConfigError):
            load_config(write_config({**VALID, "device_timeout_s": True}))

    def test_timeout_string_rejected(self):
        with self.assertRaises(ConfigError):
            load_config(write_config({**VALID, "device_timeout_s": "3"}))

    def test_ttl_bool_rejected(self):
        with self.assertRaises(ConfigError):
            load_config(write_config({**VALID, "capability_ttl_s": True}))

    def test_ttl_float_rejected(self):
        with self.assertRaises(ConfigError):
            load_config(write_config({**VALID, "capability_ttl_s": 5.5}))

    def test_bad_log_level_rejected(self):
        with self.assertRaises(ConfigError):
            load_config(write_config({**VALID, "log_level": "LOUD"}))

    def test_non_string_log_level_rejected(self):
        with self.assertRaises(ConfigError):
            load_config(write_config({**VALID, "log_level": 7}))


class ExampleFileTest(unittest.TestCase):
    def test_example_file_is_placeholder_only_and_loads(self):
        # C-L006-9: the tracked example must load cleanly and contain no
        # credentials (v1 has none — LAN-trust) and no real host assumptions
        # beyond the placeholder device hostname.
        example = Path(__file__).resolve().parents[1] / "hugo_adapter" / "config.example.json"
        config = load_config(str(example))
        self.assertEqual(config.device_url, "http://led-wall.local")
        self.assertEqual(config.warnings, ())
        text = example.read_text(encoding="utf-8")
        for secret_marker in ("password", "token", "secret", "api_key"):
            self.assertNotIn(secret_marker, text)


if __name__ == "__main__":
    unittest.main()
