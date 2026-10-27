#!/usr/bin/env python3
"""Render captured renderer frames (PPM, from tests/capture_v41_frames.sh) as LED-style PNG images.

    bash tests/capture_v41_frames.sh /tmp/frames
    python3 tools/render_frames.py /tmp/frames docs/assets/frames   # needs Pillow

Each pixel becomes a round LED on a dark panel; the 150-cell tail rope is drawn
as a strip under the 30x10 matrix when both files of a frame exist.
"""
import pathlib
import sys

from PIL import Image, ImageDraw, ImageFilter

CELL = 24
TAIL_CELL = 9
BG = (12, 14, 20)


def read_ppm(path):
    data = path.read_bytes()
    parts = data.split(maxsplit=4)
    width, height = int(parts[1]), int(parts[2])
    pixels = parts[4][: width * height * 3]
    return width, height, pixels


def draw_leds(draw, width, height, pixels, cell, x0, y0):
    for y in range(height):
        for x in range(width):
            i = (y * width + x) * 3
            r, g, b = pixels[i], pixels[i + 1], pixels[i + 2]
            if r == g == b == 0:
                r, g, b = 28, 30, 38   # unlit LED
            pad = cell * 0.14
            draw.ellipse([x0 + x * cell + pad, y0 + y * cell + pad,
                          x0 + (x + 1) * cell - pad, y0 + (y + 1) * cell - pad], fill=(r, g, b))


def main(src, dest):
    src, dest = pathlib.Path(src), pathlib.Path(dest)
    dest.mkdir(parents=True, exist_ok=True)
    for matrix in sorted(src.glob("*-matrix.ppm")):
        name = matrix.name[: -len("-matrix.ppm")]
        mw, mh, mpix = read_ppm(matrix)
        tail = src / f"{name}-tail.ppm"
        tw, th, tpix = read_ppm(tail) if tail.exists() else (0, 0, b"")
        width = max(mw * CELL, tw * TAIL_CELL) + 2 * CELL
        height = mh * CELL + 2 * CELL + (th * TAIL_CELL * 2 + CELL if tw else 0)
        img = Image.new("RGB", (width, height), BG)
        draw = ImageDraw.Draw(img)
        draw_leds(draw, mw, mh, mpix, CELL, (width - mw * CELL) // 2, CELL)
        if tw:
            draw_leds(draw, tw, th, tpix, TAIL_CELL, (width - tw * TAIL_CELL) // 2, mh * CELL + 2 * CELL)
        glow = img.filter(ImageFilter.GaussianBlur(6))
        Image.blend(glow, img, 0.7).save(dest / f"{name}.png")
        print(dest / f"{name}.png")


if __name__ == "__main__":
    main(*sys.argv[1:3])
