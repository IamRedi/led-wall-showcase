# LED WALL V1 SOURCE MAP — current path → asset → future role → action

Source root: `~/Documents/PlatformIO/Projects/PLUTO EQ ACCESSORY`
Action classes: `COPY/ADAPT` · `REWRITE CLEANLY` · `PRESERVE DATA` · `COMPATIBILITY LAYER` ·
`REFERENCE ONLY` · `ARCHIVE` · `DISCARD`

## Firmware sources

| Current path | Asset | Future role | Action |
|---|---|---|---|
| `include/eq_screen.h` | serpentine XY mapping + flip flags (`screenToIndex/screenSet/screenGet`) — **preserves verified hardware/mapping behavior** | `firmware/src/render/` | COPY/ADAPT |
| `include/font5x7.h` | 5×7 bitmap font (`Font5x7::glyphRows`) — **preserves verified text capability** | `firmware/src/text/` | COPY/ADAPT |
| `src/main.cpp` text functions (`glyphCols`, `drawChar`, `drawString`, `stringWidth`) | text rendering | `firmware/src/text/` | COPY/ADAPT |
| `src/eq_presets.cpp` + `include/eq_presets.h` | 8 spectrum style renderers (Rows/Columns/Mirror/Rainbow/PeakDots/CenterOut/Blocks/Fire) | `firmware/src/effects/` registry entries | COPY/ADAPT |
| `src/main.cpp` animation/demo functions (`demoCorners`, `demoChecker`, `demoBorder`, `demoRowSweep`, `demoBounce`, `demoWave`, `demoRain`, `demoPlasma`, carousel) | animation renderers | `firmware/src/effects/` registry entries | COPY/ADAPT |
| `include/eq_render.h` + `src/eq_render.cpp` | dirty-flag ~20 FPS throttle (`kLedMaxFpsMs`) — **preserves verified render behavior** | `firmware/src/render/` | COPY/ADAPT |
| `src/eq_audio.cpp` + `include/eq_audio.h` | mic capture/FFT/band pipeline (`captureSamples/computeBands`, attack/decay) | `firmware/src/audio/` (conditional capability) | COPY/ADAPT |
| `src/eq_piano_viz.cpp` + `include/eq_piano_viz.h` | note→column MIDI visualization (MIDI 21–108) | effects registry entry, made source-agnostic | COPY/ADAPT |
| `src/main.cpp` Paint mode + `include/eq_app.h` paint API (`paintSet/paintDraw`) | diagnostic pixel painting | diagnostic capability (`paint.diagnostic`) | COPY/ADAPT |
| `include/eq_app.h` + `src/eq_app.cpp` | mode/preset globals (`AppMode`, `EqPreset`, `g_app`) | `firmware/src/core/` state model (baseMode/transient) | REWRITE CLEANLY |
| `src/main.cpp` (mode machine `runAppMode`, serial CLI `handleSerialLine`, `setup/loop`, 800 lines flat) | app skeleton | `firmware/src/core/` + `firmware/src/api/` | REWRITE CLEANLY |
| `include/eq_config.h` | verified constants (pins, 30×10, timings, brightness, FFT config) | `firmware/include/` config + `hardware/NOTES.md` | PRESERVE DATA |
| `src/eq_pi_link.cpp` + `include/eq_pi_link.h` | WS client to Pi :8766 (reconnect, midi/spectrum/playback parsing) | optional internal/compat event adapter — NOT the product API | COMPATIBILITY LAYER (optional) |
| `include/eq_wifi_config.h` | WiFi/WS endpoint resolution incl. hard-coded `192.168.1.73:8766` and cross-repo secrets chain | own isolated config; hard-coded IP removed | DISCARD (replace) |
| `include/eq_secrets.example.h` (+ any local secrets file) | WiFi credentials pattern | own `secrets.example.h` | DISCARD (replace) |
| `src/eq_portal.cpp` + `include/eq_portal.h` | AP provisioning portal (compiled out) | — | ARCHIVE |
| `platformio.ini` | build env `esp32_eq` (esp32dev, FastLED, WebSockets, ArduinoJson) | root `firmware/platformio.ini` | COPY/ADAPT |
| `.pio/build/esp32_eq/` | build artifacts (firmware.bin 2026-05-26 — evidence of compilation) | proof of prior build only | DISCARD |

## Related evidence (not part of LED Wall, recorded for context)

| Current path | Asset | Future role | Action |
|---|---|---|---|
| `PLUTO PIANO BRAIN/src/led_test/*` (196-LED keyboard) | piano keyboard firmware | governed by Piano Core planning (`PIANO_CORE_V1_PLANNING/`) | OUT OF SCOPE here |
| `RediOn/hardware/pluto-piano-keyboard-led/` | old 320-LED keyboard skeleton | superseded; historical | REFERENCE ONLY |
| `RediOn/docs/tasks/TASK-112-pluto-7in-display-led-roadmap.md` | plan evidence for the 10-strip wall display | `docs/` history | REFERENCE ONLY |

Rule: no file migrates automatically; every COPY/ADAPT happens only after implementation
authorization, strictly against `LED_WALL_V1_FROZEN_SPEC.md`.
