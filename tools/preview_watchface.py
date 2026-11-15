#!/usr/bin/env python3
"""Draw a preview of our watch face as a PNG, the way the watch would.

Uses the generated watchface/*.h data and mirrors Adafruit GFX's text drawing,
so the result matches the watch pixel for pixel (for the parts drawn here).

Run from the repository root:
  python3 tools/preview_watchface.py out.png "09:47" "15 Nov 26" "4213" [--scale 3]
"""
import re
import sys
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
WF = ROOT / "watchface"


def load_font():
    t = (WF / "PocketPixel.h").read_text()
    bm = [int(x, 16) for x in re.search(r"Bitmaps\[\] PROGMEM = \{\s*(.*?)\s*\};", t, re.S).group(1).split(",")]
    gl = re.findall(r"\{\s*(\d+), (\d+), (\d+), (\d+), (-?\d+), (-?\d+) \}\s*/\* 0x([0-9A-F]+)", t)
    return bm, {chr(int(c, 16)): tuple(map(int, g)) for *g, c in gl}


def load_layout():
    t = (WF / "face_layout.h").read_text()
    return {k: int(v) for k, v in re.findall(r"#define (\w+)\s+(-?\d+)", t)}


def load_background():
    t = (WF / "face_background.h").read_text()
    data = [int(x, 16) for x in re.search(r"PROGMEM = \{\s*(.*?)\s*\};", t, re.S).group(1).split(",")]
    img = Image.new("1", (200, 200), 1)
    px = img.load()
    for y in range(200):
        for x in range(200):
            if data[y * 25 + x // 8] & (0x80 >> (x % 8)):
                px[x, y] = 0
    return img


class Canvas:
    def __init__(self, img, font):
        self.px, (self.bm, self.g) = img.load(), font

    def bounds(self, text, sx, sy):
        """Adafruit_GFX::getTextBounds for a single line starting at (0, 0)."""
        x, minx, maxx = 0, 1 << 15, -(1 << 15)
        for ch in text:
            o, w, h, a, xo, yo = self.g[ch]
            if w and h:
                minx, maxx = min(minx, x + xo * sx), max(maxx, x + (xo + w) * sx - 1)
            x += a * sx
        return minx, maxx - minx + 1

    def print(self, text, x, y, sx, sy):
        for ch in text:
            o, w, h, a, xo, yo = self.g[ch]
            for i in range(w * h):
                if self.bm[(o * 8 + i) // 8] & (0x80 >> ((o * 8 + i) % 8)):
                    r, c = divmod(i, w)
                    for dy in range(sy):
                        for dx in range(sx):
                            X, Y = x + (xo + c) * sx + dx, y + (yo + r) * sy + dy
                            if 0 <= X < 200 and 0 <= Y < 200:
                                self.px[X, Y] = 0
            x += a * sx

    def centred(self, text, sx, sy, cx, bottom):
        x1, w = self.bounds(text, sx, sy)
        self.print(text, cx - w // 2 - x1, bottom + 1, sx, sy)


def render(time_text, date_text, steps_text):
    L = load_layout()
    img = load_background()
    cv = Canvas(img, load_font())
    cv.centred(time_text, 6, 7, (L["TIME_LEFT"] + L["TIME_RIGHT"] + 1) // 2, L["TIME_BOTTOM"])
    cv.centred(date_text, 2, 2, (L["DATE_LEFT"] + L["DATE_RIGHT"] + 1) // 2, L["DATE_BOTTOM"])
    cv.print(steps_text, L["STEPS_LEFT"], L["STEPS_BOTTOM"] + 1, 2, 2)
    return img


if __name__ == "__main__":
    args = sys.argv[1:]
    scale = 1
    if "--scale" in args:
        i = args.index("--scale")
        scale = int(args[i + 1])
        del args[i:i + 2]
    out, time_text, date_text, steps_text = args
    img = render(time_text, date_text, steps_text)
    if scale > 1:
        img = img.resize((200 * scale, 200 * scale), Image.NEAREST)
    img.save(out)
    print("Wrote", out)
