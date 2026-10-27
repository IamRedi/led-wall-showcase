#!/usr/bin/env bash
# LED host unit tests (C-L002-1/C-L003-1/C-L004-1/C-L005-1 in docs/VERIFICATION.md).
# Pure modules only: Arduino/FastLED/Preferences-dependent files are device-side.
# The LED-L004 dashboard JS-core suite runs separately via node when available.
# The LED-L005 adapter suites run via python3 when available (C-L005-1/C-L005-2).
set -euo pipefail
cd "$(dirname "$0")/.."

CXX="${CXX:-g++}"
OUT="$(mktemp /tmp/led_l002_tests.XXXXXX)"
trap 'rm -f "$OUT"' EXIT

# -Werror makes the compile double as the static-warning check (C-L002-2).
"$CXX" -std=c++17 -Wall -Wextra -Werror \
  -Ifirmware/src -Ifirmware/include \
  -o "$OUT" \
  tests/test_main.cpp \
  tests/test_matrix_mapping.cpp \
  tests/test_mode_machine.cpp \
  tests/test_transient_timer.cpp \
  tests/test_transient_rollover.cpp \
  tests/test_frame_throttle.cpp \
  tests/test_public_state.cpp \
  tests/test_brightness.cpp \
  tests/test_config.cpp \
  tests/test_registry.cpp \
  tests/test_command_dispatcher.cpp \
  tests/test_identity.cpp \
  tests/test_ui_dashboard.cpp \
  tests/test_font5x7.cpp \
  tests/test_text_renderer.cpp \
  tests/test_render_animations.cpp \
  tests/test_expression_map.cpp \
  tests/test_renderer_selection.cpp \
  tests/test_renderer_contract.cpp \
  tests/test_render_adversarial.cpp \
  tests/test_countdown.cpp \
  tests/test_input.cpp \
  tests/test_local_menu.cpp \
  tests/test_games.cpp \
  tests/test_text_scroll_goldens.cpp \
  tests/test_text_studio.cpp \
  tests/test_visualizer.cpp \
  tests/test_api_extensions.cpp \
  tests/test_display_topology.cpp \
  tests/test_tail_games.cpp \
  tests/test_players.cpp \
  tests/test_ds4_wired.cpp \
  tests/test_led_l010.cpp \
  tests/test_repair_cycle1.cpp \
  tests/test_v41_fireworks.cpp \
  tests/test_v41_output_limiter.cpp \
  tests/test_v41_combined.cpp \
  tests/test_v41_pixel_canvas.cpp \
  tests/test_v41_games.cpp \
  tests/test_v41_scene.cpp \
  tests/test_v41_input_sessions.cpp \
  tests/test_v41_sound.cpp \
  firmware/src/core/mode_machine.cpp \
  firmware/src/core/brightness.cpp \
  firmware/src/core/config.cpp \
  firmware/src/core/config_load.cpp \
  firmware/src/api/registry.cpp \
  firmware/src/api/command.cpp \
  firmware/src/api/identity.cpp \
  firmware/src/api/ui_content.cpp \
  firmware/src/render/display_topology.cpp \
  firmware/src/render/body_topology.cpp \
  firmware/src/render/output_limiter.cpp \
  firmware/src/pixel/pixel_canvas.cpp \
  firmware/src/scene/scene_timeline.cpp \
  firmware/src/sound/sound_scheduler.cpp \
  firmware/src/text/font5x7.cpp \
  firmware/src/text/text_renderer.cpp \
  firmware/src/text/text_style.cpp \
  firmware/src/timer/countdown.cpp \
  firmware/src/input/input_events.cpp \
  firmware/src/input/input_sessions.cpp \
  firmware/src/input/button_decoder.cpp \
  firmware/src/input/ds4_core.cpp \
  firmware/src/input/wired_controller.cpp \
  firmware/src/menu/local_menu.cpp \
  firmware/src/games/game_host.cpp \
  firmware/src/games/games_impl.cpp \
  firmware/src/games/tail_games.cpp \
  firmware/src/games/v41_games.cpp \
  firmware/src/visualizer/spectrum_input.cpp \
  firmware/src/effects/animation_renderers.cpp \
  firmware/src/effects/combined_renderers.cpp \
  firmware/src/effects/tail_renderers.cpp \
  firmware/src/effects/expression_map.cpp \
  firmware/src/render/renderer.cpp

"$OUT"

# LED-L004 dashboard JS core (C-L004-6 payload/gating/state assertions).
# The suite tests the exact artifact embedded in ui_content.cpp. If node is
# unavailable the check is recorded NOT_RUN with the reason (catalog rule).
if command -v node >/dev/null 2>&1; then
  node tests/test_ui_core.mjs
else
  echo "NOT_RUN tests/test_ui_core.mjs: node is not available on this host"
fi

# LED-L009 repair cycle 3 (V-L009-4): dashboard BOOT regression suite —
# executes the exact served application script in a DOM/network sandbox
# (fresh-load exception guard, Home activation, tabs, difficulty commands,
# /state + WebSocket init, offline indicators, wired controls). Fails on the
# pre-repair RC2 artifact with the recorded TypeError.
if command -v node >/dev/null 2>&1; then
  node tests/test_ui_boot.mjs
else
  echo "NOT_RUN tests/test_ui_boot.mjs: node is not available on this host"
fi

# LED-L005 Home Assistant adapter (C-L005-1 host suites incl. the fake
# broker/device integration round-trip; C-L005-2 static compile pass).
# If python3 is unavailable the checks are recorded NOT_RUN with the reason.
if command -v python3 >/dev/null 2>&1; then
  python3 -m compileall -q ha_adapter
  python3 tests/test_nvs_provision.py
  for suite in test_adapter_config test_adapter_mqtt_proto test_adapter_device \
               test_adapter_entities test_adapter_integration; do
    python3 "tests/${suite}.py"
  done
else
  echo "NOT_RUN adapter suites: python3 is not available on this host"
fi

# LED-L006 HUGO DeviceAdapter (C-L006-1 host suites incl. the fake-device
# stdio integration round-trip; C-L006-2 static compile pass).
# If python3 is unavailable the checks are recorded NOT_RUN with the reason.
if command -v python3 >/dev/null 2>&1; then
  python3 -m compileall -q hugo_adapter
  for suite in test_hugo_config test_hugo_rpc test_hugo_intents \
               test_hugo_device test_hugo_integration; do
    python3 "tests/${suite}.py"
  done
else
  echo "NOT_RUN hugo adapter suites: python3 is not available on this host"
fi
