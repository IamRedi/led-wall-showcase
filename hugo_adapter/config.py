"""DeviceAdapter configuration: load, validate, safe defaults.

Mirrors the repository's config posture (frozen spec §18) and the accepted
LED-L005 ``ha_adapter/config.py`` pattern: explicit values win, out-of-range
numerics are clamped with a recorded warning, unknown keys are ignored with a
warning, and structurally invalid config fails closed with a deterministic
error. v1 has no credentials (documented LAN-trust; §9), so the runtime config
carries no secrets — it is still git-ignored to keep runtime state out of the
repository.
"""

from __future__ import annotations

import json
import os
from dataclasses import dataclass, field
from typing import Any, Dict, List, Optional, Tuple

_TTL_MIN_S = 1
_TTL_MAX_S = 300
_TIMEOUT_MAX_S = 30.0

_LOG_LEVELS = ("DEBUG", "INFO", "WARNING", "ERROR")


class ConfigError(Exception):
    """Deterministic configuration failure (bad file, bad shape, bad value)."""


@dataclass(frozen=True)
class AdapterConfig:
    device_url: str
    device_timeout_s: float = 3.0
    capability_ttl_s: int = 5
    log_level: str = "INFO"
    # Warnings produced while building this config (unknown keys, clamped
    # values). Surfaced via logging at startup; never contains secrets.
    warnings: Tuple[str, ...] = field(default=())


def _require_device_url(raw: Dict[str, Any]) -> str:
    value = raw.get("device_url")
    if not isinstance(value, str) or not value.strip():
        raise ConfigError("config field 'device_url' must be a non-empty string")
    device_url = value.rstrip("/")
    if not device_url.startswith("http://"):
        raise ConfigError(
            "config field 'device_url' must be an http:// URL (the device API is plain HTTP in v1)"
        )
    if len(device_url) > 256:
        raise ConfigError("config field 'device_url' exceeds 256 characters")
    _reject_userinfo(device_url)
    return device_url


def _reject_userinfo(device_url: str) -> None:
    """No credentials in the device URL: the API has no v1 authentication, and
    userinfo would leak into exception messages surfaced to the AI client."""
    authority = device_url[len("http://"):]
    if "@" in authority.split("/", 1)[0]:
        raise ConfigError("config field 'device_url' must not contain credentials (user:pass@)")


def _default_config_path() -> str:
    env = os.environ.get("LEDWALL_HUGO_CONFIG")
    if env:
        return env
    return os.path.join(os.path.dirname(os.path.abspath(__file__)), "config.json")


def load_config(path: Optional[str] = None) -> AdapterConfig:
    """Load and validate the DeviceAdapter config from a JSON file.

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

    device_url = _require_device_url(raw)

    device_timeout_s: float
    if "device_timeout_s" not in raw:
        device_timeout_s = 3.0
    else:
        value = raw["device_timeout_s"]
        if isinstance(value, bool) or not isinstance(value, (int, float)) or value <= 0 or value > _TIMEOUT_MAX_S:
            raise ConfigError(f"config field 'device_timeout_s' must be a number in (0, {int(_TIMEOUT_MAX_S)}]")
        device_timeout_s = float(value)

    ttl_raw = raw.get("capability_ttl_s")
    if ttl_raw is None:
        capability_ttl_s = 5
    else:
        if isinstance(ttl_raw, bool) or not isinstance(ttl_raw, int):
            raise ConfigError("config field 'capability_ttl_s' must be an integer")
        if ttl_raw < _TTL_MIN_S or ttl_raw > _TTL_MAX_S:
            clamped = max(_TTL_MIN_S, min(_TTL_MAX_S, ttl_raw))
            warnings.append(f"config field 'capability_ttl_s' clamped {ttl_raw} -> {clamped}")
            capability_ttl_s = clamped
        else:
            capability_ttl_s = ttl_raw

    log_level = str(raw.get("log_level", "INFO")).upper()
    if log_level not in _LOG_LEVELS:
        raise ConfigError(f"config field 'log_level' must be one of {', '.join(_LOG_LEVELS)}")

    return AdapterConfig(
        device_url=device_url,
        device_timeout_s=device_timeout_s,
        capability_ttl_s=capability_ttl_s,
        log_level=log_level,
        warnings=tuple(warnings),
    )
