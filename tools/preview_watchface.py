#!/usr/bin/env python3
"""Draw a preview of our watch face as a PNG, the way the watch would.

Uses the generated watchface/*.h data and mirrors Adafruit GFX's text drawing,
so the result matches the watch pixel for pixel (for the parts drawn here).

Run from the repository root:
  python3 tools/preview_watchface.py out.png "09:47" "15 Nov 26" "4213" ["07:20" "16:07" 28] [--scale 3]
  (optional: sunrise time, sunset time, and the sun's step along the arc, 0-100)
  Night, with moonrise, the coming sunrise, the moon's step (sunset to sunrise) and moonset:
  python3 tools/preview_watchface.py out.png "21:45" "15 Nov 26" "9420" "12:42" "07:22" 37 "20:58" --night
  Astrology icons: --astro PHASE,PLANET,SIGN (moon phase 0-7 with 4 = full; planetary hour
  planet 0-6 in Chaldean order Saturn, Jupiter, Mars, Sun, Venus, Mercury, Moon; moon sign
  0-11 with 0 = Aries). Default 4,1,11 as in the design.
"""
import re
import sys
from pathlib import Path

from PIL import Image, ImageDraw

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

    def print(self, text, x, y, sx, sy, tight=False):
        """tight: digits 1 pixel closer together, like the watch's printTight()"""
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
            x += a * sx - (1 if tight and ch.isdigit() else 0)

    def centred(self, text, sx, sy, cx, bottom):
        x1, w = self.bounds(text, sx, sy)
        self.print(text, cx - w // 2 - x1, bottom + 1, sx, sy)


def load_icons():
    t = (WF / "face_sun.h").read_text()
    num = lambda name: int(re.search(name + r"\s+(-?\d+)", t).group(1))
    arr = lambda name: [int(x, 16) for x in re.search(name + r"\[\] PROGMEM = \{\s*(.*?)\s*\};", t, re.S).group(1).split(",")]
    pts = [tuple(map(int, p)) for p in re.findall(r"\{(-?\d+), (-?\d+)\}", t)]
    sun = (num("SUN_WIDTH"), num("SUN_HEIGHT"), arr("sun_icon"))
    moon = (num("MOON_WIDTH"), num("MOON_HEIGHT"), arr("moon_icon"))
    return sun, moon, (num("MOON_OFFSET_X"), num("MOON_OFFSET_Y")), pts


def draw_bitmap(img, x0, y0, icon):
    """Adafruit_GFX::drawBitmap: black where the icon's bits are set."""
    w, h, data = icon
    px, rb = img.load(), (w + 7) // 8
    for y in range(h):
        for x in range(w):
            if data[y * rb + x // 8] & (0x80 >> (x % 8)) and 0 <= x0 + x < 200 and 0 <= y0 + y < 200:
                px[x0 + x, y0 + y] = 0


def load_astro():
    t = (WF / "face_astro.h").read_text()
    num = lambda name: int(re.search(r"#define " + name + r"\s+(-?\d+)", t).group(1))
    def table(name):
        body = re.search(name + r"\[\d+\]\[\d+\] PROGMEM = \{(.*?)\n\};", t, re.S).group(1)
        return [[int(x, 16) for x in row.split(",") if x.strip()] for row in re.findall(r"\{([^}]*)\}", body)]
    return num, table


def draw_astro(img, phase, planet, sign):
    """Moon phase (0-7), planetary hour planet (0-6, Chaldean order) and moon sign (0-11)."""
    num, table = load_astro()
    s = num("MOON_PHASE_ICONS_SIZE")
    draw_bitmap(img, num("MOON_PHASE_X"), num("MOON_PHASE_Y"), (s, s, table("moon_phase_icons")[phase]))
    s = num("PLANET_ICONS_SIZE")
    draw_bitmap(img, num("PLANET_HOUR_X"), num("PLANET_HOUR_Y"), (s, s, table("planet_icons")[planet]))
    s = num("ZODIAC_ICONS_SIZE")
    draw_bitmap(img, num("MOON_SIGN_X"), num("MOON_SIGN_Y"), (s, s, table("zodiac_icons")[sign]))


def render(time_text, date_text, steps_text, line1="07:20", line2="16:07", step="0", line3="--:--", night=False,
           astro=(4, 1, 11)):
    """Day: line1/line2 are sunrise/sunset and step places the sun.
    Night: line1/line2/line3 are moonrise/next sunrise/moonset and step places the moon
    (how far from sunset to sunrise)."""
    L = load_layout()
    img = load_background()
    cv = Canvas(img, load_font())
    cv.centred(time_text, 6, 7, (L["TIME_LEFT"] + L["TIME_RIGHT"] + 1) // 2, L["TIME_BOTTOM"])
    cv.centred(date_text, 2, 2, (L["DATE_LEFT"] + L["DATE_RIGHT"] + 1) // 2, L["DATE_BOTTOM"])
    cv.print(steps_text, L["STEPS_LEFT"], L["STEPS_BOTTOM"] + 1, 2, 2)
    if night:  # WatchFace::replaceLabel()
        draw = ImageDraw.Draw(img)
        for key, label in (("LABEL1", "Moonrise"), ("LABEL2", "Sunrise"), ("LABEL3", "Moonset")):
            draw.rectangle((L[key + "_LEFT"], L[key + "_TOP"], L[key + "_RIGHT"], L[key + "_BOTTOM"]), fill=1)
            x1, w = cv.bounds(label, 2, 2)
            cv.print(label, L[key + "_RIGHT"] + 1 - w - x1, L[key + "_BOTTOM"] + 1, 2, 2)
    cv.print(line1, L["SUNRISE_LEFT"], L["SUNRISE_BOTTOM"] + 1, 2, 2, tight=True)
    cv.print(line2, L["SUNSET_LEFT"], L["SUNSET_BOTTOM"] + 1, 2, 2, tight=True)
    if night:
        cv.print(line3, L["MOONSET_LEFT"], L["MOONSET_BOTTOM"] + 1, 2, 2, tight=True)
    draw_astro(img, *astro)
    sun, moon, (mx, my), pts = load_icons()
    if step != "-":
        x, y = pts[int(step)]
        if night:
            draw_bitmap(img, x + mx, y + my, moon)
        else:
            draw_bitmap(img, x, y, sun)
    return img


if __name__ == "__main__":
    args = sys.argv[1:]
    scale = 1
    if "--scale" in args:
        i = args.index("--scale")
        scale = int(args[i + 1])
        del args[i:i + 2]
    night = "--night" in args
    if night:
        args.remove("--night")
    astro = (4, 1, 11)  # the design's: full moon, Jupiter's hour, moon in Pisces
    if "--astro" in args:
        i = args.index("--astro")
        astro = tuple(int(v) for v in args[i + 1].split(","))
        del args[i:i + 2]
    out, time_text, date_text, steps_text, *rest = args
    img = render(time_text, date_text, steps_text, *rest, night=night, astro=astro)
    if scale > 1:
        img = img.resize((200 * scale, 200 * scale), Image.NEAREST)
    img.save(out)
    print("Wrote", out)
