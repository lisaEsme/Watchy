# CLAUDE.md

## Repository relationship

This repository (`lisaEsme/Watchy`) is a fork of the upstream project `sqfmi/Watchy`.

## Never push to upstream

**Never push, open pull requests, or send any changes from this fork to the upstream repository `sqfmi/Watchy`** (or any other repository it was forked from). This applies to commits, branches, tags, pull requests, issues, and comments.

- All pushes go to `origin` (`lisaEsme/Watchy`) only.
- Pulling or fetching *from* upstream to keep the fork updated is fine.
- If an `upstream` remote is added for syncing, disable pushing to it:

  ```bash
  git remote set-url --push upstream DISABLED-do-not-push-to-upstream
  ```

- When creating a pull request, the base repository must be `lisaEsme/Watchy`, never `sqfmi/Watchy`.

## This repository is public

Never commit WiFi names or passwords, API keys, or personal details. The WiFi network goes in
`watchface/wifi_secrets.h`, which is gitignored. Never commit built firmware either: `firmware/`
and `.pio/` are gitignored.

## Our project: a custom watch face

The work lives in `watchface/`. Start by reading `watchface/README.md`, which explains what the
face shows, how it works and how accurate it is, and `watchface/design/README.md` for the design
and asset pipeline.

- **Hardware:** Watchy **v2.0**: ESP32-PICO-D4, USB-C, `/dev/ttyACM0` on the user's Linux
  machine. It is *not* a v3, despite the USB-C port. Build for `ARDUINO_WATCHY_V20`, never the
  ESP32-S3 / v3 target.
- **Toolchain:** PlatformIO (`platformio.ini`, env `watchy-v20`), espressif32 6.9.0, which is
  Arduino-ESP32 core **2.0.17** (the code does not support core 3.x). Libraries are pinned to
  exact commits. `Rtc_Pcf8563` must stay on `7e2ece1`, because newer commits fail on core
  2.0.17. Keep each library's dependencies listed before it in `lib_deps`.
  `pio-lib/library.json` builds `src/` without the unpinned dependencies in the top-level
  `library.json`.
- **CI:** `.github/workflows/firmware.yml` builds the firmware on push and uploads the
  `watchy-firmware` artifact (a merged `.bin` to flash at `0x0`).
- **Flashing:** the user flashes with esptool from `~/esptool-venv`:
  `--chip esp32 --port /dev/ttyACM0 --baud 921600 write-flash 0x0 <file>.bin`.
  Merged images put the bootloader at `0x1000`, partitions at `0x8000`, `boot_app0` at `0xe000`
  and the app at `0x10000`.

### User preferences

- **Location:** Cambridge, UK. **Time zone:** UK GMT/BST via the POSIX rule in `settings.h`.
  **Clock:** 24-hour.
- **Astrology:** **tropical** zodiac, **traditional** planetary hours (Chaldean order; the
  planetary day starts at sunrise).
- **Buzzing:** no buzz on the clock hour (`vibrateOClock = false`). One very slight 40 ms buzz
  when a planetary hour begins.
- **Time sync:** manual (Sync Time) is fine; no automatic nightly WiFi sync.
- **Battery:** bottom-right battery icon (4 bars, hysteresis), low-battery mode below 3.60 V
  (redraw every 5 min, "LOW"). CPU at 80 MHz. Battery-saving ideas the user has seen but that
  clash with the night features (sleeping at night, refreshing less often all the time,
  wrist-tilt wake) are not implemented; ask before adding them.
- **Night mode** (sunset to sunrise): the moon icon on the arc shows progress from sunset to
  sunrise. The lines read Moonrise / Sunrise / Moonset. Moonrise keeps the actual rise time while
  the moon is still up after midnight, and moves to the new date's moonrise only once it has set.
- **Design:** the face design is the user's (GIMP, 200 × 200, 1-bit, Pocket Pixel font). Keep
  the face matching `watchface/design/face.png`, and check changes with the preview tool.

### Working on the face

- **Changed the design or icons:** run `python3 tools/make_watchface_assets.py`
  (needs `pillow fonttools`). It regenerates `watchface/face_*.h` and `PocketPixel.h`; never
  edit those by hand.
- **Before handing over firmware:** render a preview with `python3 tools/preview_watchface.py`
  and compare it with `design/face.png`. Outside the live areas it should differ by zero pixels
  (expected exceptions: one pixel near the sun, from the cleaner `sun.png`, and the battery icon
  in the bottom-right corner, which isn't in the design).
- **Astronomy changes:** test the header-only code on the host
  (`g++ -I watchface test.cpp`, using `timegm` in place of TimeLib's `makeTime`), and compare
  with independent references: `astral`, `ephem` (PyEphem) and Swiss Ephemeris (`pyswisseph`
  needs `setuptools<70` and `--no-build-isolation`). Don't rely on a single reference: `astral`
  was wrong about moonrise on 24 Oct 2027.
- **Arduino `.ino` files:** keep code in `.cpp` files. Without Arduino's own `ctags`, the
  `.ino` preprocessor can insert broken prototypes.
- **This repository's `.gitattributes`** marks `*.png`, `*.ttf`, `*.bin` and `*.jpg` as binary.
  Keep it that way.

- **Menus:** the library menu has "Font Sizes" (library) and, via the virtual
  `extraMenuItem(i)` / `showExtraMenuItem(i)` hooks (up to `MAX_EXTRA_MENU_ITEMS`), our
  face's "Planets" page (degree, sign, R for retrograde; whole degrees) and "Lucky" page.
  A face page that sets `guiState = EXTRA_PAGE_STATE` gets Up/Down through
  `extraMenuItemButton(i, step)`. With 8 items the menu spacing drops from 25 to 22 px, with
  9 to 21 px.
- **Lucky moments:** `watchface/lucky_windows.h` is the user's own table, generated outside
  this repository (`fortune_timer.py` / `make_watchy_header.py`). Use it as given: don't
  recompute the astrology on the watch or edit the table by hand. It is personal (from her
  birth chart), so it is **gitignored and must never be committed**; CI gets it from the
  `LUCKY_WINDOWS_H_GZ` secret (gzip + base64), and without it builds the empty
  `lucky_windows.example.h`. Both Part of Fortune
  formulas show as "Fortuna"; Tyche as "Tyche". Scores of 9+ are the standouts.
  On the face, a star after the date marks a lucky day (outline: any moment that UK day;
  filled: "super lucky", a score of 9+). The stars are `design/icons/16x16/*lucky_day_16x16.png`
  (her pick, 18 Nov 2026); preview with `--lucky lucky|super`.

- **Screens that stay up while the watch waits for a button** (menu pages): call
  `display.powerOff()` straight after `display.display(...)`. The driver leaves the panel's
  high voltage on after a refresh, and large black areas then fade to white within seconds
  (seen on the watch, fixed for Font Sizes and Planets). Use full refreshes for pages with
  large solid shapes; partial refreshes leave ghosts.

### Ideas the user has mentioned but not asked for yet

- Quiet hours for the planetary-hour buzz (e.g. none at night).
- Sun sign and planet signs on the face itself, using the unused icons in `watchface/design/icons/`
  (the Planets menu page already lists them in text).
- A "tomorrow" marker on moon times.
