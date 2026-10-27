# ADR-LED-L011: V4.1 product canvas and two-zone programs

- Status: Authorized for LED-L011 software implementation; independent review and Owner acceptance pending.
- Date: 2026-10-15
- Owner: Redion Cekrezi
- Task: LED-L011
- Authorization: Owner's request to implement the supplied V4.1 execution contract in this conversation.

## Context

The frozen V1 specification defines a single 30×10 semantic display, with paint limited to diagnostics and no multi-zone layout or normal raw-coordinate API (§5, §23). LED-L010 added a Tail zone under ADR-LED-L010 but retained the prohibition on a phone framebuffer API for its own milestone. V4.1 now explicitly asks for a 450-cell Pixel Studio, local Save/Send/Live, and coordinated Matrix/Tail programs. This is an architectural amendment to the product contract, not a reinterpretation of V1 or an acceptance of LED-L010.

## Decision

1. V1 frozen files stay byte-identical as historical architecture evidence. From LED-L011 onward, this ADR overlays only the conflicting V1 paint/multi-zone constraints and LED-L010's task-specific non-scope. Existing V1 semantic clients and legacy identifiers remain valid.
2. The phone may submit one complete, bounded RGB art snapshot through a local-only, explicit Pixel Studio capability. It receives a device ACK with session and revision. This authority does not extend to HUGO, Home Assistant, GPIO, FastLED, raw electric indices, arbitrary streams or automatic reconnect replay. The ESP32 owns the active canvas and applies the existing output ceiling.
3. Logical coordinates are Matrix `(x,y)` in the established 30×10 orientation and Tail `t=0..149` from the matrix anchor toward the free end. The existing driver reverse transform maps logical Tail coordinates to physical LEDs. The anchor is a configurable visual coordinate between the two center columns, never a claimed extra LED or a rewiring instruction.
4. Game, text, scene, animation, pixel canvas and timer foreground ownership must be explicit. Rendering produces the two logical surfaces as one device-owned frame; outputs may latch at different times. Real physical synchronization and power adequacy require separate device evidence.
5. Device project storage is enabled only after the existing filesystem and capacity are verified. Save is explicit, bounded and atomic. The browser draft is independent and exportable; it is not a guaranteed backup.
6. No physical flash, wiring, power or buzzer operation is authorized by this ADR. DS4/WiFi coexistence and electrical gates retain their recorded blocker states.

## Compatibility and proof

The LED-L011 implementation record must bind the exact source snapshot, new API schema and response errors, resource use, deterministic frame/input tests and UI behavior. Independent review and Owner acceptance are separate. Any proposed change outside these decisions requires another Owner-authorized amendment.
