#!/usr/bin/env bash
# Capture exact RGB frames from production C++ renderers/games/canvas.
set -euo pipefail
cd "$(dirname "$0")/.."
dest="${1:-docs/assets/frames}"
mkdir -p "$dest"
tool_bin="$(mktemp /tmp/led_v41_frames.XXXXXX)"
trap 'rm -f "$tool_bin"' EXIT
"${CXX:-g++}" -std=c++17 -Wall -Wextra -Werror -Ifirmware/src -Ifirmware/include \
  -o "$tool_bin" tests/capture_v41_frames.cpp \
  firmware/src/effects/combined_renderers.cpp \
  firmware/src/effects/animation_renderers.cpp \
  firmware/src/effects/tail_renderers.cpp \
  firmware/src/games/game_host.cpp \
  firmware/src/games/games_impl.cpp \
  firmware/src/games/tail_games.cpp \
  firmware/src/games/v41_games.cpp \
  firmware/src/pixel/pixel_canvas.cpp \
  firmware/src/render/body_topology.cpp \
  firmware/src/render/display_topology.cpp \
  firmware/src/text/font5x7.cpp \
  firmware/src/text/text_renderer.cpp \
  firmware/src/text/text_style.cpp \
  firmware/src/timer/countdown.cpp
"$tool_bin" "$dest"
sha256sum "$dest"/*.ppm
