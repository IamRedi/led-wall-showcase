# LED Wall

[![CI](https://github.com/IamRedi/led-wall-showcase/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/IamRedi/led-wall-showcase/actions/workflows/ci.yml)

**An LED wall as a standalone device.** An ESP32 drives a 30 × 10 WS2812 matrix and a second, freely routed LED rope ("tail", 150 cells). It shows text, scenes, animations, facial expressions, a countdown and 14 one- or two-player games. It is controlled from a web UI served by the device itself, from a DualShock 4 controller, or from outside through a tightly bounded API.

LED Wall is the second device of the HUGO family: [HUGO](https://github.com/IamRedi/hugo-core-showcase), the author's AI assistant, can use the wall as a face and display without ever touching a single LED. What is proven about that is listed under [Role in the HUGO ecosystem](#role-in-the-hugo-ecosystem).

[Deutsche Version](README.md) · [Project status (German)](docs/PROJEKTSTAND.md) · [Evidence log (German)](EVIDENCE.md) · [Security](SECURITY.md) · [API contract](docs/api/CONTRACT.md)

| Device-served UI (phone) | Pong, frame from the real renderer | Fireworks across matrix and tail |
|---|---|---|
| ![V5 web UI home screen on a phone](docs/assets/v5-home-mobile.png) | ![Pong frame: two paddles, ball, center line](docs/assets/frames/pong-100.png) | ![Fireworks frame with tail rope](docs/assets/frames/fireworks-2750.png) |

The phone image is a screenshot of the UI served by the real device. The LED images are exact frames from the firmware code (`tests/capture_v41_frames.sh`, drawn as LEDs by `tools/render_frames.py`), not photos of the wall.


> **Evidence and limitations:** The published snapshot was verified using host-based tests without a connected ESP32 or physical LEDs. Operation of the physical LED wall is based on the author's records; formal electrical measurement and acceptance are still outstanding. [Verification record (German)](EVIDENCE.md).

## At a glance

| | |
|---|---|
| **What** | Firmware and control for an LED wall: text, 45 scenes/animations, 12 expressions, 14 games, countdown/timer, pixel editor, notifications, standby |
| **Hardware** | ESP32 (4 MB flash), 300 WS2812 on GPIO13 (10 serpentine strips of 30), tail rope on GPIO27, separate LED power supply, optional DualShock 4 over Bluetooth |
| **Stack** | C++17 on Arduino/PlatformIO (FastLED, WebSockets, ArduinoJson; DS4 via Bluepad32), build-free web UI stored in flash, Python 3 standard library only for the adapters |
| **Interfaces** | Local HTTP/WebSocket API, Home Assistant adapter (MQTT), HUGO adapter (JSON-RPC over stdio) |
| **Tests** | 44,552 C++ checks, 79 JavaScript checks, 269 Python tests, all green ([evidence log](EVIDENCE.md)) |
| **Status** | Runs as a real device at the author's home and, per the owner (27 Oct 2026), works reliably. Not a commercial product; a formal electrical measurement is pending ([status](docs/PROJEKTSTAND.md)) |

## What it does

- **One device, two surfaces.** The matrix and the tail rope play together or separately; effects such as fireworks, meteor impact or storm run across both.
- **Games.** Pong, Snake, Breakout, Reaction, Dodger, Color Chain, Light Race, Reaction Stop, Color Catch, Memory, Tug of War, Tail Tennis, Light Quest, Quick Draw. Two players via phone and DualShock 4 controller, with player sessions, heartbeat and pause on connection loss.
- **Text and time.** Scrolling 5 × 7 text, a text studio, countdown and timer on the device.
- **One authority.** The device alone decides what is shown: exactly one base mode, at most one transient overlay, no queue. `clear` always blanks everything. Brightness has a hard ceiling no client can exceed.
- **No app needed.** The web UI (six areas: scenes, games, draw, text, timer, device) is stored compressed in flash and works on any phone on the Wi-Fi.
- **Other systems are just clients.** Home Assistant and HUGO use the same semantic API as the phone; the wall works without them.

## Role in the HUGO ecosystem

HUGO is the author's personal AI assistant. [HUGO Core](https://github.com/IamRedi/hugo-core-showcase) owns identity, memory and permissions; devices such as LED Wall, [HUGO Eye](https://github.com/IamRedi/hugo-eye-showcase) or an ESP32 rover stay independent. The rule: **the AI states an intent, the device renders deterministically.**

**In the code and tested:**

- `hugo_adapter/` is a standalone adapter (Python, standard library only) that offers the wall as a fixed tool list: `show_text`, `clear_text`, `set_scene`, `set_animation`, `set_expression`, `set_brightness`, `clear_display`, `get_state`, `get_device_info`, over JSON-RPC 2.0 on stdin/stdout ([hugo_adapter/README.md](hugo_adapter/README.md)).
- The expression vocabulary is made for an assistant: `idle`, `listening`, `thinking`, `speaking`, `happy`, `alert`, `error`, `sleeping` (plus `bored`, `tired`, `focused`, `excited`), so HUGO can show on the wall whether it is listening, thinking or speaking.
- HUGO never sees LED indexes, GPIO, FastLED or renderer functions. Arguments are validated before any device call and rejected on error; tools the device does not advertise disappear instead of guessing. Discovery does not mean permission.
- 158 tests for the HUGO adapter against an emulated device (config, device client, intents, JSON-RPC, integration). The firmware contains zero HUGO code.

**Not proven yet:** the adapter has never run against a live HUGO Core or against the real wall; it is verified with an emulated device. It also speaks its own JSON-RPC format; binding it to HUGO Core's capability contract (which [HUGO Eye](https://github.com/IamRedi/hugo-eye-showcase) already implements) is a separate open step.

## Try it without hardware

Requirements: g++ (C++17), Python 3.10+, Node.js 20+.

```bash
git clone https://github.com/IamRedi/led-wall-showcase && cd led-wall-showcase
bash tests/run_tests.sh                       # all host tests: C++, JavaScript, Python

# render frames from the real renderers and draw them as LEDs (needs Pillow)
bash tests/capture_v41_frames.sh /tmp/frames
python3 tools/render_frames.py /tmp/frames /tmp/frames-png
```

## Building the firmware

```bash
pip install platformio
pio run -e esp32_eq -d firmware               # standard firmware
pio run -e esp32_eq_ds4 -d firmware           # with DualShock 4 (Bluepad32)
```

Wi-Fi credentials are not in the source but in the device's NVS storage, and the firmware never returns them through the API. `tools/nvs_provision.py` edits hardware settings only (e.g. tail length) in an NVS image file, never on the device directly.

## License

Source available for reading and evaluation, not for reuse: see [LICENSE](LICENSE). Libraries such as FastLED, WebSockets, ArduinoJson and Bluepad32 are fetched by PlatformIO at build time and keep their own licenses.
