# Our Watchy watch face

This is our own watch face for the Watchy, built on top of the SQFMI Watchy library in `src/`.
It shows the time, the date, the step count, the sun and moon, and some astrology.
The design is ours (`design/face.png`). This file explains what the face shows, how it works,
and how to build it.

## The watch

- **Hardware:** Watchy **v2.0**: ESP32-PICO-D4 chip, USB-C port, with a separate USB-serial chip
  that appears on Linux as `/dev/ttyACM0`. It looks like the newer v3 because of the USB-C port,
  but `esptool` reports an ESP32-PICO-D4, which only the v1 to v2 boards have. The buttons
  confirmed v2.0: on v2.0, Up is on pin 35.
- **Location and time zone:** Cambridge, UK (`settings.h`), on UK time: GMT in winter and
  BST in summer.

## What the face shows

**Always:** the time (24-hour) and the date, in the Pocket Pixel font, and today's step count
(reset at midnight).

**By day (sunrise to sunset):**
- **Sunrise** and **Sunset**: today's times.
- **The arc:** the sun icon travels from the left end at sunrise to the right end at sunset, in
  proportion to how much of the daylight has passed. At solar noon it is at the top.

**At night (sunset to the next sunrise):**
- **Moonrise**: while the moon is up, when it rose (even if that was the day before); once it has
  set, the moonrise on the date shown (`--:--` on the roughly monthly days with none).
- **Sunrise**: the coming sunrise.
- **Moonset**: the moonset after the moonrise shown. This line only appears at night.
- **The arc:** the moon icon travels from the left end at sunset to the right end at sunrise,
  counting down to the sun's return.

At sunrise the face switches back to the day face.

**Astrology (tropical zodiac, traditional planetary hours):**
- **Big disc:** the moon's phase, one of 8 pictures.
- **Under the seven-pointed star:** the planet ruling the current planetary hour. A single,
  very short buzz (40 ms) marks the start of each new planetary hour.
- **Under the crescent:** the sign the moon is in.

**Not used:** the default on-the-hour buzz is switched off (`vibrateOClock = false`).

## Building and flashing

You need the watch plugged in by USB. There are three ways:

1. **Build on your computer with PlatformIO.** Use VS Code with the PlatformIO extension, or the
   command line:
   ```bash
   pio run -t upload
   ```
   `platformio.ini` is set up for this watch: espressif32 6.9.0 (Arduino-ESP32 core 2.0.17),
   the `watchy` pin variant, `ARDUINO_WATCHY_V20`, and every library pinned to a tested commit.
   The first build downloads about 1–2 GB of tools.
2. **Let GitHub build it.** Every push that changes the firmware runs the "Build Watchy firmware"
   workflow. Download the `watchy-firmware` artifact from the Actions tab, and flash its
   `Watchy-firmware-v20.bin` (step 3). Cloud builds never include your WiFi password.
3. **Flash a ready-made `.bin`** with esptool (installed in its own Python environment,
   `~/esptool-venv`):
   ```bash
   ~/esptool-venv/bin/python -m esptool --chip esp32 --port /dev/ttyACM0 --baud 921600 write-flash 0x0 Watchy-firmware-v20.bin
   ```
   No button needs holding: the USB chip resets the watch into download mode itself.

After flashing, use **Sync Time** in the watch's menu to set the time over WiFi.

## Settings (`settings.h`)

- **Location:** `LAT` / `LON`, used for weather, sunrise and sunset, moonrise and moonset, and
  planetary hours.
- **Time zone:** `TIMEZONE` is a POSIX rule, `GMT0BST,M3.5.0/1,M10.5.0` for the UK. The clock
  itself only changes between GMT and BST when you use **Sync Time**, so sync after the clocks
  change in March and October. Sunrise, sunset and the moon times switch by themselves.
- **WiFi:** put the network name and password in `wifi_secrets.h` (copy
  `wifi_secrets.example.h`). That file is in `.gitignore` and must never be committed, because
  this repository is public. Without it, the watch uses the network saved through the
  **Setup WiFi** menu.

## How it works

| Feature | File | Method |
|---|---|---|
| Sunrise and sunset | `sun.h` | NOAA Solar Calculator (Meeus), refined at the time of each event |
| Moonrise and moonset | `moon.h` | Astronomical Almanac low-precision moon, plus a search for the horizon crossing |
| Moon and sun longitude | `astro.h` | Meeus ch. 47 (main ELP-2000/82 terms) plus nutation; Meeus ch. 25 for the sun |
| Moon sign | `astro.h` | Tropical: longitude ÷ 30° |
| Moon phase | `astro.h` | The moon's angle from the sun, split into 8 equal parts centred on new, quarters and full |
| Planetary hours | `astro.h` | Daylight and night each split into 12 from sunrise and sunset; Chaldean order from the day's ruler; the planetary day starts at sunrise |
| Local times | `main.cpp` | The `TIMEZONE` rule through the C library (`setenv("TZ")`, `localtime_r`, `mktime`) |

Moon times are saved in memory that survives deep sleep (`RTC_DATA_ATTR`). They're only
recalculated when the date changes or the moon rises or sets, so the minute-by-minute
wake-ups stay short.

### How accurate it is

These are checked against independent sources, running the same code on a computer:

- **Sunrise and sunset:** within 23 seconds of the `astral` library on 600 days in 2026–2030.
- **Moonrise and moonset:** within 3.5 minutes (median 0.1) on 90 dates, including days without a
  moonrise and the 23- and 25-hour days when the clocks change. On the one date where `astral`
  reported no moonrise (24 Oct 2027), PyEphem agreed with ours.
- **Moon longitude:** within 13 arcseconds of **Swiss Ephemeris** at 2,000 moments in 2025–2035,
  with no disagreement about the sign. The sun is within 35 arcseconds. All 160 moon sign
  changes in 2027 land within 11 seconds of Swiss Ephemeris.

## Files

| Path | What it is |
|---|---|
| `main.cpp` | The watch face: what is drawn where, day and night logic, the planetary-hour buzz |
| `settings.h` | Location, time zone, WiFi, weather settings |
| `sun.h`, `moon.h`, `astro.h` | The astronomy (no internet needed) |
| `design/` | The design (`face.png`), the sun and moon arc icons, the icon set in `icons/`, and the Pocket Pixel font. See `design/README.md` |
| `face_*.h`, `PocketPixel.h` | Generated from `design/` by `tools/make_watchface_assets.py`. Don't edit by hand |
| `../tools/preview_watchface.py` | Draws a pixel-accurate preview PNG without flashing |
| `../pio-lib/library.json` | Compiles the Watchy library from `src/` without its unpinned dependencies |

## Credits

- **Pocket Pixel** font by stuck-in-suburbia, licensed CC BY-SA 3.0 (see `design/README.md`).
- **Watchy** library and hardware by SQFMI (`src/`, MIT licence).
- **Astronomical methods:** Jean Meeus, *Astronomical Algorithms*; NOAA Solar Calculator;
  Astronomical Almanac.
