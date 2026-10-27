"""Intent catalog for the HUGO DeviceAdapter — the frozen spec §16 boundary.

The catalog is fixed data: the tool surface is exactly what §16/ADR-014 allow
HUGO to use (text, scene, animation, expression, brightness, clear, state —
plus spectrum/color where capability-flagged). No tool exposes LED indexes,
GPIO, FastLED, renderer functions, or microphone internals; the AI client
requests intent, the device renders.

Every tool validates its arguments locally and fail-closed before any device
call: malformed, missing, extra, or wrongly-typed input raises
:class:`InvalidArguments` and never reaches the device. Registry membership
(scene/animation/expression names) stays device-authoritative — names are
sanity-bounded locally, forwarded verbatim, and the §9 typed error with its
available list is surfaced to the caller by the service layer.
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import Any, Callable, Dict, Optional, Tuple

# §5/§6/CONTRACT.md bounds (independently re-derived in the test suites).
TEXT_MIN_CHARS = 1
TEXT_MAX_CHARS = 128
SPEED_MIN = 40
SPEED_MAX = 500
SPEED_DEFAULT = 140
DURATION_MIN_MS = 1
DURATION_MAX_MS = 2**31 - 1  # wrap-safe deadline arithmetic ceiling (CONTRACT.md)
BRIGHTNESS_MIN = 0
BRIGHTNESS_MAX = 100
NAME_MIN_CHARS = 1
NAME_MAX_CHARS = 64
HUE_MIN = 0
HUE_MAX = 359
SCHEME_MAX_CHARS = 32
STYLE_MAX_CHARS = 32
SPECTRUM_SOURCE = "local_mic"

# Frozen vocabularies (spec §7/§8) embedded for tool descriptions only — the
# device stays authoritative for registry membership (the API does not
# enumerate registries; documented limitation, LED-L005 precedent).
SCENE_KEYS = (
    "corners", "checker", "border", "row_sweep", "bounce", "wave", "rain", "plasma",
)
ANIMATION_KEYS = (
    "corners", "checker", "border", "row_sweep", "bounce", "wave", "rain", "plasma",
)
EXPRESSION_KEYS = (
    "idle", "listening", "thinking", "speaking", "happy", "alert", "error", "sleeping",
)
SPECTRUM_STYLES = (
    "spectrum.rows", "spectrum.columns", "spectrum.mirror", "spectrum.rainbow",
    "spectrum.peaks", "spectrum.center_out", "spectrum.blocks", "spectrum.fire",
)

# Conditional-tool capability flags (the exact capabilities[] strings the
# device advertises; see firmware identity / CONTRACT.md).
CAP_TEXT_SHOW = "display.text.show"
CAP_TEXT_CLEAR = "display.text.clear"
CAP_SCENE_SET = "display.scene.set"
CAP_ANIMATION_SET = "display.animation.set"
CAP_EXPRESSION_SET = "display.expression.set"
CAP_CLEAR = "display.clear"
CAP_BRIGHTNESS_SET = "display.brightness.set"
CAP_STATE_READ = "display.state.read"
CAP_COLOR_SET = "display.color.set"
CAP_SPECTRUM_START = "display.spectrum.start"
CAP_SPECTRUM_STOP = "display.spectrum.stop"


class InvalidArguments(Exception):
    """Locally rejected tool arguments — the device was never contacted."""


def _require_args_dict(arguments: Any) -> Dict[str, Any]:
    if arguments is None:
        return {}
    if not isinstance(arguments, dict):
        raise InvalidArguments("'arguments' must be a JSON object")
    return arguments


def _reject_unknown(arguments: Dict[str, Any], allowed: Tuple[str, ...]) -> None:
    for key in arguments:
        if key not in allowed:
            raise InvalidArguments(f"unknown argument: {key}")


def _str_arg(arguments: Dict[str, Any], key: str, required: bool, lo: int, hi: int) -> Optional[str]:
    if key not in arguments:
        if required:
            raise InvalidArguments(f"missing required argument: {key}")
        return None
    value = arguments[key]
    if not isinstance(value, str):
        raise InvalidArguments(f"argument '{key}' must be a string")
    if len(value) < lo or len(value) > hi:
        raise InvalidArguments(f"argument '{key}' must be {lo}..{hi} characters long")
    try:
        value.encode("utf-8")
    except UnicodeEncodeError:
        raise InvalidArguments(f"argument '{key}' is not UTF-8 encodable") from None
    return value


def _name_arg(arguments: Dict[str, Any], key: str) -> str:
    """Sanity-bounded registry name; membership stays device-authoritative."""
    value = _str_arg(arguments, key, True, NAME_MIN_CHARS, NAME_MAX_CHARS)
    assert value is not None  # _str_arg(required=True) never returns None
    if any(ord(ch) < 0x20 or ord(ch) == 0x7F for ch in value):
        raise InvalidArguments(f"argument '{key}' must not contain control characters")
    return value


def _bool_arg(arguments: Dict[str, Any], key: str) -> Optional[bool]:
    if key not in arguments:
        return None
    value = arguments[key]
    if not isinstance(value, bool):
        raise InvalidArguments(f"argument '{key}' must be a boolean")
    return value


def _int_arg(arguments: Dict[str, Any], key: str, required: bool, lo: int, hi: int) -> Optional[int]:
    if key not in arguments:
        if required:
            raise InvalidArguments(f"missing required argument: {key}")
        return None
    value = arguments[key]
    # bool is an int subclass; reject it explicitly.
    if isinstance(value, bool) or not isinstance(value, int):
        raise InvalidArguments(f"argument '{key}' must be an integer")
    if value < lo or value > hi:
        raise InvalidArguments(f"argument '{key}' must be between {lo} and {hi}")
    return value


def _no_args(arguments: Any) -> Dict[str, Any]:
    args = _require_args_dict(arguments)
    _reject_unknown(args, ())
    return args


# -- tool payload builders (pure: validate -> (path, payload)) ---------------


def _build_show_text(arguments: Any) -> Tuple[str, Dict[str, Any]]:
    args = _require_args_dict(arguments)
    _reject_unknown(args, ("text", "scroll", "speedMsPerPx", "durationMs"))
    payload: Dict[str, Any] = {"text": _str_arg(args, "text", True, TEXT_MIN_CHARS, TEXT_MAX_CHARS)}
    scroll = _bool_arg(args, "scroll")
    if scroll is not None:
        payload["scroll"] = scroll
    speed = _int_arg(args, "speedMsPerPx", False, SPEED_MIN, SPEED_MAX)
    if speed is not None:
        payload["speedMsPerPx"] = speed
    duration = _int_arg(args, "durationMs", False, DURATION_MIN_MS, DURATION_MAX_MS)
    if duration is not None:
        payload["durationMs"] = duration
    return "/display/text/show", payload


def _build_text_clear(arguments: Any) -> Tuple[str, Dict[str, Any]]:
    _no_args(arguments)
    return "/display/text/clear", {}


def _build_scene_set(arguments: Any) -> Tuple[str, Dict[str, Any]]:
    args = _require_args_dict(arguments)
    _reject_unknown(args, ("scene", "durationMs"))
    payload: Dict[str, Any] = {"scene": _name_arg(args, "scene")}
    duration = _int_arg(args, "durationMs", False, DURATION_MIN_MS, DURATION_MAX_MS)
    if duration is not None:
        payload["durationMs"] = duration
    return "/display/scene/set", payload


def _build_animation_set(arguments: Any) -> Tuple[str, Dict[str, Any]]:
    args = _require_args_dict(arguments)
    _reject_unknown(args, ("animation", "durationMs"))
    payload: Dict[str, Any] = {"animation": _name_arg(args, "animation")}
    duration = _int_arg(args, "durationMs", False, DURATION_MIN_MS, DURATION_MAX_MS)
    if duration is not None:
        payload["durationMs"] = duration
    return "/display/animation/set", payload


def _build_expression_set(arguments: Any) -> Tuple[str, Dict[str, Any]]:
    args = _require_args_dict(arguments)
    _reject_unknown(args, ("expression",))  # expressions are persistent: no durationMs (§5)
    return "/display/expression/set", {"expression": _name_arg(args, "expression")}


def _build_brightness_set(arguments: Any) -> Tuple[str, Dict[str, Any]]:
    args = _require_args_dict(arguments)
    _reject_unknown(args, ("level",))
    level = _int_arg(args, "level", True, BRIGHTNESS_MIN, BRIGHTNESS_MAX)
    return "/display/brightness/set", {"level": level}


def _build_clear_display(arguments: Any) -> Tuple[str, Dict[str, Any]]:
    _no_args(arguments)
    return "/display/clear", {}


def _build_state_read(arguments: Any) -> Tuple[str, Dict[str, Any]]:
    _no_args(arguments)
    return "/state", {}


def _build_identity_read(arguments: Any) -> Tuple[str, Dict[str, Any]]:
    _no_args(arguments)
    return "/identity", {}


def _build_color_set(arguments: Any) -> Tuple[str, Dict[str, Any]]:
    args = _require_args_dict(arguments)
    _reject_unknown(args, ("hue", "scheme"))
    has_hue = "hue" in args
    has_scheme = "scheme" in args
    if has_hue == has_scheme:  # exactly one of hue/scheme (§5: {hue/scheme})
        raise InvalidArguments("exactly one of 'hue' or 'scheme' is required")
    if has_hue:
        hue = _int_arg(args, "hue", True, HUE_MIN, HUE_MAX)
        return "/display/color/set", {"hue": hue}
    scheme = _str_arg(args, "scheme", True, NAME_MIN_CHARS, SCHEME_MAX_CHARS)
    assert scheme is not None
    return "/display/color/set", {"scheme": scheme}


def _build_spectrum_start(arguments: Any) -> Tuple[str, Dict[str, Any]]:
    args = _require_args_dict(arguments)
    _reject_unknown(args, ("style", "source"))
    payload: Dict[str, Any] = {"source": SPECTRUM_SOURCE}
    style = _str_arg(args, "style", False, NAME_MIN_CHARS, STYLE_MAX_CHARS)
    if style is not None:
        if any(ord(ch) < 0x20 or ord(ch) == 0x7F for ch in style):
            raise InvalidArguments("argument 'style' must not contain control characters")
        payload["style"] = style
    if args.get("source", SPECTRUM_SOURCE) != SPECTRUM_SOURCE:
        raise InvalidArguments(f"argument 'source' must be \"{SPECTRUM_SOURCE}\" in v1")
    return "/display/spectrum/start", payload


def _build_spectrum_stop(arguments: Any) -> Tuple[str, Dict[str, Any]]:
    _no_args(arguments)
    return "/display/spectrum/stop", {}


@dataclass(frozen=True)
class ToolSpec:
    """One §16 intent tool: fixed name, optional capability gate, builder."""

    name: str
    summary: str
    capability: Optional[str]  # None = core (always advertised per §10)
    http_method: str  # "POST" command or "GET" query
    build: Callable[[Any], Tuple[str, Dict[str, Any]]]


TOOL_SPECS: Tuple[ToolSpec, ...] = (
    ToolSpec(
        name="show_text",
        summary=(
            "Show text on the LED wall (persistent base mode; transient with durationMs). "
            f"Arguments: text (1..{TEXT_MAX_CHARS} chars), scroll (bool), speedMsPerPx "
            f"({SPEED_MIN}..{SPEED_MAX}, default {SPEED_DEFAULT}), durationMs (optional transient)."
        ),
        capability=None,
        http_method="POST",
        build=_build_show_text,
    ),
    ToolSpec(
        name="clear_text",
        summary="Clear text only (base text -> idle; transient text -> return to base).",
        capability=None,
        http_method="POST",
        build=_build_text_clear,
    ),
    ToolSpec(
        name="set_scene",
        summary=(
            "Set a registered scene (persistent; transient with durationMs). "
            f"Known scenes: {', '.join(SCENE_KEYS)}. Unknown names are answered by the "
            "device with the available list."
        ),
        capability=None,
        http_method="POST",
        build=_build_scene_set,
    ),
    ToolSpec(
        name="set_animation",
        summary=(
            "Set a registered animation (persistent; transient with durationMs). "
            f"Known animations: {', '.join(ANIMATION_KEYS)}."
        ),
        capability=None,
        http_method="POST",
        build=_build_animation_set,
    ),
    ToolSpec(
        name="set_expression",
        summary=(
            "Set a persistent expression intent (the device maps it internally). "
            f"Vocabulary: {', '.join(EXPRESSION_KEYS)}. No durationMs (persistent by design)."
        ),
        capability=None,
        http_method="POST",
        build=_build_expression_set,
    ),
    ToolSpec(
        name="set_brightness",
        summary=f"Set brightness level {BRIGHTNESS_MIN}..{BRIGHTNESS_MAX} (clamped by the device ceiling).",
        capability=None,
        http_method="POST",
        build=_build_brightness_set,
    ),
    ToolSpec(
        name="clear_display",
        summary="Stop everything: cancel any transient, blank the display, return to idle. Always available.",
        capability=None,
        http_method="POST",
        build=_build_clear_display,
    ),
    ToolSpec(
        name="get_state",
        summary="Read the live public device state (mode, transient, text, brightness, uptime).",
        capability=None,
        http_method="GET",
        build=_build_state_read,
    ),
    ToolSpec(
        name="get_device_info",
        summary=(
            "Read device identity: deviceId, firmware version, capabilities, logical matrix "
            "size. Discovery only — grants no control beyond the same fixed tool set."
        ),
        capability=None,
        http_method="GET",
        build=_build_identity_read,
    ),
    ToolSpec(
        name="set_color",
        summary=(
            "Set the global accent color (hue 0..359 or a named scheme) — only available when "
            "the device advertises the color capability."
        ),
        capability=CAP_COLOR_SET,
        http_method="POST",
        build=_build_color_set,
    ),
    ToolSpec(
        name="start_spectrum",
        summary=(
            "Start the local-microphone spectrum mode (optional style; "
            f"known styles: {', '.join(SPECTRUM_STYLES)}) — only available when the device "
            "advertises the spectrum capability."
        ),
        capability=CAP_SPECTRUM_START,
        http_method="POST",
        build=_build_spectrum_start,
    ),
    ToolSpec(
        name="stop_spectrum",
        summary="Stop the spectrum mode and return to the base mode — spectrum capability only.",
        capability=CAP_SPECTRUM_STOP,
        http_method="POST",
        build=_build_spectrum_stop,
    ),
)

# The §16 allowed surface, as a checkable set (tests transcribe §16 independently).
ALLOWED_TOOL_NAMES = frozenset(spec.name for spec in TOOL_SPECS)


def tool_by_name(name: str) -> Optional[ToolSpec]:
    for spec in TOOL_SPECS:
        if spec.name == name:
            return spec
    return None


def core_tools() -> Tuple[ToolSpec, ...]:
    return tuple(spec for spec in TOOL_SPECS if spec.capability is None)


def conditional_tools() -> Tuple[ToolSpec, ...]:
    return tuple(spec for spec in TOOL_SPECS if spec.capability is not None)


def tool_listing(capabilities: Optional[Tuple[str, ...]]) -> Tuple[ToolSpec, ...]:
    """Tools eligible for listing: core always; conditional only when advertised.

    ``capabilities=None`` (unknown/unverifiable) fails closed: conditional
    tools are hidden.
    """
    listed = list(core_tools())
    if capabilities is not None:
        for spec in conditional_tools():
            if spec.capability in capabilities:
                listed.append(spec)
    return tuple(listed)


def build_request(spec: ToolSpec, arguments: Any) -> Tuple[str, str, Dict[str, Any]]:
    """Validate arguments and derive the HTTP request for one tool call.

    Raises InvalidArguments (local rejection, device never contacted).
    Capability gating is the service layer's job (it owns the cache).
    """
    path, payload = spec.build(arguments)
    return spec.http_method, path, payload
