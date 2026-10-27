"""Adapter configuration: load, validate, safe defaults.

Mirrors the repository's config posture (frozen spec §18): explicit values win,
out-of-range numerics are clamped with a recorded warning, unknown keys are
ignored with a warning, and structurally invalid config fails closed with a
deterministic error. Real credentials live only in the uncommitted runtime
config file (ha_adapter/config.json, git-ignored).
"""

from __future__ import annotations

import json
import os
from dataclasses import dataclass, field
from typing import Any, Dict, List, Optional, Tuple

_POLL_INTERVAL_MIN_S = 1.0
_POLL_INTERVAL_MAX_S = 60.0
_KEEPALIVE_MIN_S = 10
_KEEPALIVE_MAX_S = 300
_PORT_MAX = 65535

# MQTT topic filters may not contain wildcards or NUL (MQTT 3.1.1 §2.3.5).
_TOPIC_FORBIDDEN = ("+", "#", "\x00")

_LOG_LEVELS = ("DEBUG", "INFO", "WARNING", "ERROR")


class ConfigError(Exception):
    """Deterministic configuration failure (bad file, bad shape, bad value)."""


@dataclass(frozen=True)
class AdapterConfig:
    device_url: str
    mqtt_host: str
    mqtt_port: int = 1883
    mqtt_username: Optional[str] = None
    mqtt_password: Optional[str] = None
    discovery_enabled: bool = True
    discovery_prefix: str = "homeassistant"
    topic_prefix: str = "ledwall"
    poll_interval_s: float = 5.0
    device_timeout_s: float = 3.0
    keepalive_s: int = 30
    log_level: str = "INFO"
    # Warnings produced while building this config (unknown keys, clamped
    # values). Surfaced via logging at startup; never contains secrets.
    warnings: Tuple[str, ...] = field(default=())

    def __repr__(self) -> str:  # secret hygiene: never render the password
        hidden = "***" if self.mqtt_password is not None else None
        return (
            f"AdapterConfig(device_url={self.device_url!r}, mqtt_host={self.mqtt_host!r}, "
            f"mqtt_port={self.mqtt_port}, mqtt_username={self.mqtt_username!r}, "
            f"mqtt_password={hidden!r}, discovery_enabled={self.discovery_enabled}, "
            f"discovery_prefix={self.discovery_prefix!r}, topic_prefix={self.topic_prefix!r}, "
            f"poll_interval_s={self.poll_interval_s}, device_timeout_s={self.device_timeout_s}, "
            f"keepalive_s={self.keepalive_s}, log_level={self.log_level!r}, "
            f"warnings={self.warnings!r})"
        )


def _require_str(raw: Dict[str, Any], key: str) -> str:
    value = raw.get(key)
    if not isinstance(value, str) or not value.strip():
        raise ConfigError(f"config field '{key}' must be a non-empty string")
    return value


def _optional_str(raw: Dict[str, Any], key: str) -> Optional[str]:
    value = raw.get(key)
    if value is None:
        return None
    if not isinstance(value, str):
        raise ConfigError(f"config field '{key}' must be a string or null")
    return value


def _validate_topic_fragment(name: str, key: str) -> str:
    if not name or any(ch in name for ch in _TOPIC_FORBIDDEN) or len(name) > 128:
        raise ConfigError(
            f"config field '{key}' must be a non-empty topic fragment without wildcards (+ #) or NUL, max 128 chars"
        )
    return name


def _default_config_path() -> str:
    env = os.environ.get("LEDWALL_HA_CONFIG")
    if env:
        return env
    return os.path.join(os.path.dirname(os.path.abspath(__file__)), "config.json")


def load_config(path: Optional[str] = None) -> AdapterConfig:
    """Load and validate the adapter config from a JSON file.

    Raises ConfigError for a missing file, invalid JSON, unknown-typed known
    fields, or invalid fixed values. Numeric ranges are clamped with warnings.
    """
    config_path = path if path is not None else _default_config_path()
    try:
        with open(config_path, "r", encoding="utf-8") as fh:
            raw = json.load(fh)
    except FileNotFoundError as exc:
        raise ConfigError(f"config file not found: {config_path}") from exc
    except (json.JSONDecodeError, UnicodeDecodeError) as exc:
        raise ConfigError(f"config file is not valid JSON: {config_path}: {exc}") from exc
    if not isinstance(raw, dict):
        raise ConfigError("config file must contain a JSON object")

    warnings: List[str] = []
    for key in sorted(raw):
        if key not in AdapterConfig.__dataclass_fields__:  # unknown keys ignored with a warning (§18 pattern)
            warnings.append(f"unknown config key ignored: {key}")

    device_url = _require_str(raw, "device_url").rstrip("/")
    if not device_url.startswith("http://"):
        raise ConfigError(
            "config field 'device_url' must be an http:// URL (the device API is plain HTTP in v1)"
        )
    if len(device_url) > 256:
        raise ConfigError("config field 'device_url' exceeds 256 characters")

    mqtt_host = _require_str(raw, "mqtt_host")

    def int_field(key: str, default: int, lo: int, hi: int, clamp: bool) -> int:
        if key not in raw:
            return default
        value = raw[key]
        if isinstance(value, bool) or not isinstance(value, (int, float)) or (
            isinstance(value, float) and not value.is_integer()
        ):
            raise ConfigError(f"config field '{key}' must be an integer")
        ivalue = int(value)
        if ivalue < lo or ivalue > hi:
            if not clamp:
                raise ConfigError(f"config field '{key}' must be between {lo} and {hi}")
            clamped = max(lo, min(hi, ivalue))
            warnings.append(f"config field '{key}' clamped {ivalue} -> {clamped}")
            ivalue = clamped
        return ivalue

    mqtt_port = int_field("mqtt_port", 1883, 1, _PORT_MAX, clamp=False)

    poll_interval_s = float(
        int_field("poll_interval_s", 5, int(_POLL_INTERVAL_MIN_S), int(_POLL_INTERVAL_MAX_S), clamp=True)
    )

    keepalive_s = int_field("keepalive_s", 30, _KEEPALIVE_MIN_S, _KEEPALIVE_MAX_S, clamp=True)

    device_timeout_s: float
    if "device_timeout_s" not in raw:
        device_timeout_s = 3.0
    else:
        value = raw["device_timeout_s"]
        if isinstance(value, bool) or not isinstance(value, (int, float)) or value <= 0 or value > 30:
            raise ConfigError("config field 'device_timeout_s' must be a number in (0, 30]")
        device_timeout_s = float(value)

    discovery_enabled = True
    if "discovery_enabled" in raw:
        if not isinstance(raw["discovery_enabled"], bool):
            raise ConfigError("config field 'discovery_enabled' must be a boolean")
        discovery_enabled = raw["discovery_enabled"]

    discovery_prefix = _validate_topic_fragment(
        str(raw.get("discovery_prefix", "homeassistant")), "discovery_prefix"
    )
    topic_prefix = _validate_topic_fragment(str(raw.get("topic_prefix", "ledwall")), "topic_prefix")

    log_level = str(raw.get("log_level", "INFO")).upper()
    if log_level not in _LOG_LEVELS:
        raise ConfigError(f"config field 'log_level' must be one of {', '.join(_LOG_LEVELS)}")

    return AdapterConfig(
        device_url=device_url,
        mqtt_host=mqtt_host,
        mqtt_port=mqtt_port,
        mqtt_username=_optional_str(raw, "mqtt_username"),
        mqtt_password=_optional_str(raw, "mqtt_password"),
        discovery_enabled=discovery_enabled,
        discovery_prefix=discovery_prefix,
        topic_prefix=topic_prefix,
        poll_interval_s=poll_interval_s,
        device_timeout_s=device_timeout_s,
        keepalive_s=keepalive_s,
        log_level=log_level,
        warnings=tuple(warnings),
    )
