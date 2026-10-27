"""Adapter config tests (C-L005-1/C-L005-8): validation, safe fallbacks,
secret hygiene."""

import json
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from ha_adapter.config import AdapterConfig, ConfigError, load_config  # noqa: E402

VALID = {
    "device_url": "http://led-wall.local/",
    "mqtt_host": "broker.local",
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
        self.assertEqual(config.mqtt_host, "broker.local")
        self.assertEqual(config.mqtt_port, 1883)
        self.assertTrue(config.discovery_enabled)
        self.assertEqual(config.discovery_prefix, "homeassistant")
        self.assertEqual(config.topic_prefix, "ledwall")
        self.assertEqual(config.poll_interval_s, 5.0)
        self.assertEqual(config.device_timeout_s, 3.0)
        self.assertEqual(config.keepalive_s, 30)
        self.assertEqual(config.log_level, "INFO")
        self.assertEqual(config.warnings, ())
        self.assertIsNone(config.mqtt_username)
        self.assertIsNone(config.mqtt_password)

    def test_full_config_round_trip(self):
        config = load_config(write_config({
            "device_url": "http://10.0.0.8",
            "mqtt_host": "10.0.0.1",
            "mqtt_port": 8883,
            "mqtt_username": "user",
            "mqtt_password": "secret",
            "discovery_enabled": False,
            "discovery_prefix": "ha",
            "topic_prefix": "wall",
            "poll_interval_s": 10,
            "device_timeout_s": 1.5,
            "keepalive_s": 60,
            "log_level": "debug",
        }))
        self.assertEqual(config.device_url, "http://10.0.0.8")
        self.assertEqual(config.mqtt_port, 8883)
        self.assertEqual(config.mqtt_username, "user")
        self.assertEqual(config.mqtt_password, "secret")
        self.assertFalse(config.discovery_enabled)
        self.assertEqual(config.log_level, "DEBUG")
        self.assertEqual(config.poll_interval_s, 10.0)
        self.assertEqual(config.device_timeout_s, 1.5)

    def test_missing_required_field_fails_closed(self):
        for key in ("device_url", "mqtt_host"):
            data = dict(VALID)
            del data[key]
            with self.subTest(key=key):
                with self.assertRaises(ConfigError):
                    load_config(write_config(data))

    def test_missing_file_fails_closed(self):
        with self.assertRaises(ConfigError):
            load_config("/nonexistent/ledwall-ha-config.json")

    def test_invalid_json_fails_closed(self):
        with self.assertRaises(ConfigError):
            load_config(write_config("{broken"))

    def test_non_object_json_fails_closed(self):
        with self.assertRaises(ConfigError):
            load_config(write_config([1, 2]))

    def test_wrong_types_fail_closed(self):
        cases = [
            {**VALID, "device_url": 5},
            {**VALID, "mqtt_port": "1883"},
            {**VALID, "mqtt_port": True},
            {**VALID, "discovery_enabled": "yes"},
            {**VALID, "poll_interval_s": "soon"},
            {**VALID, "keepalive_s": 1.5},
            {**VALID, "device_timeout_s": -1},
            {**VALID, "device_timeout_s": 100},
            {**VALID, "log_level": "VERBOSE"},
            {**VALID, "mqtt_password": 123},
        ]
        for data in cases:
            with self.subTest(data=data):
                with self.assertRaises(ConfigError):
                    load_config(write_config(data))

    def test_out_of_range_integers_fail_closed_or_clamp(self):
        with self.assertRaises(ConfigError):  # ports fail closed, no clamp
            load_config(write_config({**VALID, "mqtt_port": 70000}))
        config = load_config(write_config({**VALID, "poll_interval_s": 0}))
        self.assertEqual(config.poll_interval_s, 1.0)  # clamped with warning
        self.assertTrue(any("poll_interval_s" in w for w in config.warnings))
        config = load_config(write_config({**VALID, "poll_interval_s": 5000}))
        self.assertEqual(config.poll_interval_s, 60.0)
        config = load_config(write_config({**VALID, "keepalive_s": 1}))
        self.assertEqual(config.keepalive_s, 10)

    def test_unknown_keys_ignored_with_warning(self):
        config = load_config(write_config({**VALID, "wifi_extra": "ignored"}))
        self.assertEqual(config.warnings, ("unknown config key ignored: wifi_extra",))

    def test_topic_fragments_reject_wildcards(self):
        for key in ("topic_prefix", "discovery_prefix"):
            with self.subTest(key=key):
                with self.assertRaises(ConfigError):
                    load_config(write_config({**VALID, key: "led+wall"}))
                with self.assertRaises(ConfigError):
                    load_config(write_config({**VALID, key: "led#wall"}))
                with self.assertRaises(ConfigError):
                    load_config(write_config({**VALID, key: ""}))

    def test_only_http_device_url_accepted(self):
        for url in ("https://led-wall", "ftp://led-wall", "led-wall.local", ""):
            with self.subTest(url=url):
                with self.assertRaises(ConfigError):
                    load_config(write_config({"device_url": url, "mqtt_host": "b"}))


class SecretHygieneTest(unittest.TestCase):
    def test_password_never_appears_in_repr(self):
        config = load_config(write_config({**VALID, "mqtt_password": "hunter2"}))
        rendered = repr(config)
        self.assertNotIn("hunter2", rendered)
        self.assertIn("***", rendered)

    def test_warnings_do_not_leak_values(self):
        config = load_config(write_config({**VALID, "mqtt_password": "hunter2", "poll_interval_s": 0}))
        self.assertNotIn("hunter2", repr(config.warnings))


class DirectConstructionTest(unittest.TestCase):
    """Tests construct AdapterConfig directly for fast poll intervals."""

    def test_direct_dataclass_allows_fast_poll(self):
        config = AdapterConfig(device_url="http://127.0.0.1:1", mqtt_host="127.0.0.1",
                               poll_interval_s=0.05)
        self.assertEqual(config.poll_interval_s, 0.05)


if __name__ == "__main__":
    unittest.main()
