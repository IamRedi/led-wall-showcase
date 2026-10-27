# LED WALL V1 AUDIT SUMMARY — evidence base for the frozen architecture

Documentation only; no code migrated. Companion to `LED_WALL_V1_FROZEN_SPEC.md`.

## Project / evidence source

- Project: `~/Documents/PlatformIO/Projects/PLUTO EQ ACCESSORY`
- PlatformIO environment: `esp32_eq` (env `esp32dev`, Arduino, FastLED)
- Identified as the remembered LED wall/panel project by the LED Wall Panel Discovery
  Audit: the only 30×10 / 300-LED / 10-strip matrix project on the machine; firmware
  build artifact `.pio/build/esp32_eq/firmware.bin` dated 2026-05-26.
- Related non-matches ruled out by the same audit: `RediOn/hardware/pluto-piano-keyboard-led`
  (320-LED keyboard skeleton, superseded) and `PLUTO PIANO BRAIN/src/led_test`
  (196-LED piano keyboard) — neither is the panel.
- RediON roadmap corroboration: `docs/tasks/TASK-112-pluto-7in-display-led-roadmap.md`
  plans the "10 vertical strips (equalizer on the wall)" as a future layer.

## USER/REPOSITORY VERIFIED hardware

- ESP32 devkit family (`esp32dev`), Arduino framework
- WS2812 LEDs (FastLED, GRB)
- 300 LEDs total
- 30 × 10 matrix (30 wide, 10 high)
- 10 physical rows/strips × 30 LEDs
- Data pin GPIO13
- Serpentine layout (row 0: 0→29; odd rows reversed), runtime flip flags
  (`eq_screen.h::screenToIndex`, `EQ_FLIP_ODD_ROWS` / `EQ_FLIP_Y`)
- GPIO34 analog microphone heritage (9 kHz sampling, 256-sample FFT)
- Brightness heritage 40 (`EQ_BRIGHTNESS`)
- ~20 FPS dirty-frame render throttle (`eq_render.cpp`, `kLedMaxFpsMs = 50`)
- Brownout detector disabled at boot (strip+WiFi USB-power heritage)

## Existing functionality worth preserving

- 5×7 bitmap font (`include/font5x7.h`)
- Scrolling text (`glyphCols/drawChar/drawString/stringWidth` in `src/main.cpp`)
- Matrix XY mapping (`include/eq_screen.h`)
- 13 heritage animations/demos (`demoCorners…demoPlasma`, carousel) in `src/main.cpp`
- 8 spectrum styles (`src/eq_presets.cpp`: Rows, Columns, Mirror, Rainbow, PeakDots,
  CenterOut, Blocks, Fire)
- Microphone FFT pipeline (`src/eq_audio.cpp`, attack/decay smoothing)
- Paint mode (`paintSet/paintDraw`) — diagnostic
- Pi MIDI visualization (`src/eq_piano_viz.cpp`, note→column over MIDI 21–108)
- Render throttle (dirty-flag + 50 ms floor)
- WiFi/reconnect patterns (`src/eq_pi_link.cpp`, 8 s backoff)
- Serial diagnostics CLI (bring-up channel)

## New V1 architecture functionality

- Semantic HTTP (commands/queries) + WebSocket (events) API
- Device identity/version/capabilities handshake (incl. logical matrix size)
- baseMode + one-transient state model (replace-not-queue, no stacks)
- `display.expression.set` as V1 CORE (8-intent vocabulary, internal mappings)
- Typed error contract (`textTooWide`, `unknownScene`, `unknownExpression`, …)
- Public scene/animation registry with stable semantic names
- Brightness with device-enforced physical ceiling
- HUGO/LOOI DeviceAdapter boundary
- Home Assistant external adapter boundary
- Standalone device identity (no Piano Core/Pi dependency)

## Technical debt intentionally discarded

- Hard-coded Piano Core/Pi IP dependency (`EQ_WS_HOST 192.168.1.73:8766`)
- Shared PLUTO secrets/includes (`eq_wifi_config.h` → `pluto_secrets.h` cross-repo chain)
- Serial-only product control (serial becomes diagnostic-only)
- Passive Piano dependency as the only "smart" input
- Dead AP portal as current architecture (`eq_portal.cpp`, compiled out)
- Flat mixed demo/product `main.cpp` (800 lines)
- "EQ accessory" naming as product identity (the device is a display)
- Magic configuration values (palette indices, timing constants)
- Undocumented brownout workaround assumed as the power solution

**Legacy PLUTO EQ ACCESSORY is an evidence and migration source, not the future LED Wall architecture.**
