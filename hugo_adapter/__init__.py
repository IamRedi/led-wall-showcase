"""LED Wall HUGO DeviceAdapter (host side, LED-L006).

External adapter exposing the LED Wall's semantic API (docs/api/CONTRACT.md)
as a bounded, deterministic, intent-only tool surface for HUGO-class clients
(frozen spec §16 / ADR-014). Zero firmware changes; the device stays fully
functional without this adapter.
"""

__all__ = ["config", "device", "intents", "rpc", "service"]
