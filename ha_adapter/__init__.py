"""Home Assistant external adapter for LED Wall (LED-L005).

A pure client of the LED Wall semantic HTTP API (LED-L003, docs/api/CONTRACT.md)
that exposes the device to Home Assistant over MQTT per ADR-015/frozen spec §17.
MQTT never enters the firmware; the device remains authoritative for all visual
state.
"""

__version__ = "1.0.0"
