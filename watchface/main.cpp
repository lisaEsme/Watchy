// Our own watch face, designed in watchface/design/face.png.
// Build and upload with PlatformIO (see platformio.ini in the repository root).
//
// Live now: time (24-hour), date and step count. By day, today's sunrise and sunset,
// with the sun travelling along the arc; at night, moonrise and the coming sunrise,
// with the moon travelling along the arc from sunset to sunrise, plus moonset.
// Astrology (tropical zodiac, traditional planetary hours): the moon's phase in the
// big disc, the planet ruling the current planetary hour under the seven-pointed
// star (with a gentle buzz when it changes), and the moon's sign under the crescent.
// See design/face-annotated.png.
#include <Arduino.h>
#include <Watchy.h>

#include "settings.h"
#include "PocketPixel.h"
#include "face_background.h"
#include "face_layout.h"
#include "face_sun.h"
#include "astro.h"
#include "face_astro.h"
#include "moon.h"
#include "lucky.h"
#include "planets.h"
#include "sun.h"
#include <time.h>

extern long gmtOffset; // current offset from UTC in seconds, kept by the Watchy library

static const char *MONTHS[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                               "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

class WatchFace : public Watchy {
public:
  explicit WatchFace(const watchySettings &s) : Watchy(s) {}

  void drawWatchFace() override {
    display.fillScreen(GxEPD_WHITE);
    display.drawBitmap(0, 0, face_background, 200, 200, GxEPD_BLACK);
    display.setFont(&PocketPixel);
    display.setTextColor(GxEPD_BLACK);

    char text[16];

    // Time, e.g. "09:05", centred where the design has it
    snprintf(text, sizeof(text), "%02d:%02d", currentTime.Hour, currentTime.Minute);
    drawCentred(text, 6, 7, (TIME_LEFT + TIME_RIGHT + 1) / 2, TIME_BOTTOM);

    // Date, e.g. "15 Nov 26"
    snprintf(text, sizeof(text), "%d %s %02d", currentTime.Day,
             MONTHS[(currentTime.Month + 11) % 12], (currentTime.Year + 1970) % 100);
    drawCentred(text, 2, 2, (DATE_LEFT + DATE_RIGHT + 1) / 2, DATE_BOTTOM);

    // Steps, reset at midnight, after the "Steps" label
    if (currentTime.Hour == 0 && currentTime.Minute == 0) {
      sensor.resetStepCounter();
    }
    snprintf(text, sizeof(text), "%lu", (unsigned long)sensor.getCounter());
    display.setTextSize(2);
    display.setCursor(STEPS_LEFT, STEPS_BOTTOM + 1); // cursor sits on the baseline
    display.print(text);

    drawSun();
    drawAstro();
    drawBattery();

    display.setTextSize(1); // the menus share this display, so leave it at normal size
  }

  // The "Font Sizes" menu shows our face font, Pocket Pixel
  const GFXfont *fontSizesFont() override { return &PocketPixel; }

  // Our menu items: "Planets" and "Lucky" (see showPlanets() and showLucky())
  const char *extraMenuItem(int index) override {
    static const char *ITEMS[] = {"Planets", "Lucky"};
    return index < 2 ? ITEMS[index] : nullptr;
  }

  void showExtraMenuItem(int index) override {
    if (index == 0) {
      showPlanets();
    } else {
      showLucky(LUCKY_OPEN);
    }
  }

  // Up/Down on the Lucky page turn its pages
  void extraMenuItemButton(int index, int step) override {
    if (index == 1) {
      showLucky(step < 0 ? LUCKY_UP : LUCKY_DOWN);
    }
  }

  // Menu item "Planets": where each planet is in the (tropical) zodiac right now
  void showPlanets() {
    static const char *NAMES[] = {"Sun", "Moon", "Mercury", "Venus", "Mars",
                                  "Jupiter", "Saturn", "Uranus", "Neptune", "Pluto"};
    static const char *SIGNS[] = {"Ari", "Tau", "Gem", "Can", "Leo", "Vir",
                                  "Lib", "Sco", "Sag", "Cap", "Aqu", "Pis"};
    RTC.read(currentTime);
    time_t now = nowUTC();

    display.setFullWindow();
    display.fillScreen(GxEPD_WHITE);
    display.setFont(&PocketPixel);
    display.setTextColor(GxEPD_BLACK);
    display.setTextSize(2);

    // Header: "Planets" and the time the positions are for
    char text[16];
    display.setCursor(PLANETS_NAME_X, 13);
    display.print("Planets");
    snprintf(text, sizeof(text), "%02d:%02d", currentTime.Hour, currentTime.Minute);
    printRightAligned(text, 197, 13);

    // One line per body: name, degree within the sign, sign, and R if retrograde
    for (int b = 0; b < planets::COUNT; b++) {
      int16_t baseline = 13 + 18 + 17 * b;
      double lon = planets::longitude((planets::Body)b, now);
      int sign = (int)(lon / 30.0) % 12;
      int degree = (int)(lon - sign * 30.0); // whole degrees, as astrologers write them

      display.setCursor(PLANETS_NAME_X, baseline);
      display.print(NAMES[b]);
      snprintf(text, sizeof(text), "%d", degree);
      printRightAligned(text, PLANETS_DEGREE_RIGHT, baseline);
      // Degree mark: Pocket Pixel has no degree sign, so draw a small ring
      display.drawRect(PLANETS_DEGREE_RIGHT + 2, baseline - 12, 4, 4, GxEPD_BLACK);
      display.setCursor(PLANETS_SIGN_X, baseline);
      display.print(SIGNS[sign]);
      if (planets::retrograde((planets::Body)b, now)) {
        display.setCursor(PLANETS_RETRO_X, baseline);
        display.print("R");
      }
    }

    display.setTextSize(1);
    display.display(false);
    display.powerOff(); // don't leave the high voltage on while the page waits (see showFontSizes)
    guiState = APP_STATE; // Back returns to the menu
  }

  // ---- Lucky moments -------------------------------------------------------

  // Menu item "Lucky": the coming lucky moments from lucky_windows.h, grouped by day,
  // each with its time, Tyche or Fortuna (Part of Fortune) and its score. The header
  // counts down to the next one. Up/Down turn the pages; it always opens on the next
  // moment. Moments stay listed until 2 minutes after their time (they are +/- 2 min).
  enum LuckyAction { LUCKY_OPEN, LUCKY_UP, LUCKY_DOWN };
  static const int LUCKY_ROWS = 10; // rows under the header
  static const int16_t LUCKY_TIME_X = 10, LUCKY_NAME_X = 62;
  static RTC_DATA_ATTR int luckyFirst; // first moment on the page shown
  static RTC_DATA_ATTR int luckyNext;  // first moment after it (for Down)

  // Rows needed to list moments [from, to): one per moment, plus a heading for each day
  int luckyRows(int from, int to) {
    int rows = 0;
    long day = -1;
    for (int i = from; i < to; i++) {
      long d = localDayNumber(LUCKY_MOMENTS[i].utc);
      rows += (d != day) ? 2 : 1;
      day = d;
    }
    return rows;
  }

  void showLucky(LuckyAction action) {
    RTC.read(currentTime);
    time_t now = nowUTC();
    int upcoming = lucky::firstUpcoming(now);

    if (action == LUCKY_OPEN || luckyFirst < upcoming) {
      luckyFirst = upcoming;
    } else if (action == LUCKY_DOWN) {
      if (luckyNext < LUCKY_MOMENT_COUNT) {
        luckyFirst = luckyNext;
      }
    } else { // Up: the earliest start whose page still reaches the current first moment
      int start = luckyFirst;
      while (start > upcoming && luckyRows(start - 1, luckyFirst) <= LUCKY_ROWS) {
        start--;
      }
      luckyFirst = start;
    }

    display.setFullWindow();
    display.fillScreen(GxEPD_WHITE);
    display.setFont(&PocketPixel);
    display.setTextColor(GxEPD_BLACK);
    display.setTextSize(2);

    // Header: "Lucky" and how long until the next moment
    char text[24];
    display.setCursor(2, 13);
    display.print("Lucky");
    if (upcoming < LUCKY_MOMENT_COUNT) {
      formatCountdown((long)LUCKY_MOMENTS[upcoming].utc - (long)now, text, sizeof(text));
      printRightAligned(text, 197, 13);
    } else {
#ifdef LUCKY_TABLE_MISSING
      static const char *LINES[] = {"No table:", "built without", "lucky_windows.h", nullptr,
                                    "See README.md"};
#else
      static const char *LINES[] = {"No more moments:", "the table has", "run out.", nullptr,
                                    "Regenerate", "lucky_windows.h"};
#endif
      for (size_t l = 0; l < sizeof(LINES) / sizeof(LINES[0]); l++) {
        if (LINES[l]) {
          display.setCursor(2, luckyRow(l));
          display.print(LINES[l]);
        }
      }
    }

    // The moments, with a heading for each day ("Today", "Tomorrow" or e.g. "Sun 13 Dec").
    // A heading never goes on the last row on its own.
    int row = 0, i = luckyFirst;
    long day = -1;
    long today = localDayNumber(now);
    while (i < LUCKY_MOMENT_COUNT) {
      const LuckyMoment &m = LUCKY_MOMENTS[i];
      long d = localDayNumber(m.utc);
      if (d != day) {
        if (row + 2 > LUCKY_ROWS) {
          break;
        }
        drawLuckyDayHeading(m.utc, d - today, luckyRow(row++));
        day = d;
      } else if (row + 1 > LUCKY_ROWS) {
        break;
      }
      int16_t baseline = luckyRow(row++);
      formatLocalTime(m.utc, text, sizeof(text));
      display.setCursor(LUCKY_TIME_X, baseline);
      display.print(text);
      display.setCursor(LUCKY_NAME_X, baseline);
      display.print(lucky::pointName(m.point));
      snprintf(text, sizeof(text), "%d", m.score);
      printRightAligned(text, 197, baseline);
      i++;
    }
    luckyNext = i;

    display.setTextSize(1);
    display.display(false);
    display.powerOff(); // don't leave the high voltage on while the page waits (see showFontSizes)
    guiState = EXTRA_PAGE_STATE; // Up/Down turn the pages, Back returns to the menu
  }

  static int16_t luckyRow(int row) { return 13 + 18 + 17 * row; }

  // A day's heading, with a line from the text to the right edge
  void drawLuckyDayHeading(time_t utc, long daysAway, int16_t baseline) {
    static const char *DAYS[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    char text[16];
    if (daysAway == 0) {
      strcpy(text, "Today");
    } else if (daysAway == 1) {
      strcpy(text, "Tomorrow");
    } else {
      struct tm local;
      toLocal(utc, local);
      snprintf(text, sizeof(text), "%s %d %s", DAYS[local.tm_wday], local.tm_mday,
               MONTHS[local.tm_mon]);
    }
    display.setCursor(2, baseline);
    display.print(text);
    int16_t x = display.getCursorX() + 4;
    display.drawFastHLine(x, baseline - 6, 198 - x, GxEPD_BLACK);
  }

  // "now", "in 12m", "in 4h05m" (under two days), otherwise "in 25d"
  static void formatCountdown(long seconds, char *out, size_t size) {
    long minutes = (seconds + 59) / 60;
    if (seconds <= lucky::SLACK) {
      snprintf(out, size, "now");
    } else if (minutes < 60) {
      snprintf(out, size, "in %ldm", minutes);
    } else if (minutes < 48 * 60) {
      snprintf(out, size, "in %ldh%02ldm", minutes / 60, minutes % 60);
    } else {
      snprintf(out, size, "in %ldd", minutes / (24 * 60));
    }
  }

  // The local (UK) calendar day a UTC time falls on, as a day count (for comparing days)
  long localDayNumber(time_t utc) {
    struct tm local;
    toLocal(utc, local);
    return (long)(utcMidnight(local.tm_year + 1900, local.tm_mon + 1, local.tm_mday) / 86400);
  }

  static const int16_t PLANETS_NAME_X = 2, PLANETS_DEGREE_RIGHT = 98, PLANETS_SIGN_X = 110,
                       PLANETS_RETRO_X = 150;

  void printRightAligned(const char *text, int16_t right, int16_t baseline) {
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
    display.setCursor(right + 1 - (int16_t)w - x1, baseline);
    display.print(text);
  }

  // Low-battery mode: only redraw every 5 minutes (the clock then shows the time
  // to the last 5 minutes; the planetary-hour buzz may come up to 4 minutes late)
  bool updateFaceOnTick() override {
    return !lowBattery || currentTime.Minute % 5 == 0;
  }

private:
  // Moon phase, moon sign and planetary hour (see astro.h)
  void drawAstro() {
    float lat = settings.lat.toFloat(), lon = settings.lon.toFloat();
    time_t now = nowUTC();

    // Moon phase: the nearest of the eight phase pictures (0 = new, 4 = full)
    int phase = (int)floor(astro::moonAge(now) * 8 + 0.5) % 8;
    display.drawBitmap(MOON_PHASE_X, MOON_PHASE_Y, moon_phase_icons[phase],
                       MOON_PHASE_ICONS_SIZE, MOON_PHASE_ICONS_SIZE, GxEPD_BLACK);

    // The sign the moon is in (tropical)
    int moonSign = astro::sign(astro::moonLongitude(now));
    display.drawBitmap(MOON_SIGN_X, MOON_SIGN_Y, zodiac_icons[moonSign], ZODIAC_ICONS_SIZE,
                       ZODIAC_ICONS_SIZE, GxEPD_BLACK);

    // Planetary hour. The planetary day starts at sunrise, so before dawn we are
    // still in yesterday's planetary day (its night hours).
    time_t midnightUTC = utcMidnight(currentTime.Year + 1970, currentTime.Month, currentTime.Day);
    time_t rise, set, nextRise, unused;
    if (!sunTimes(midnightUTC, lat, lon, rise, set)) {
      return; // no sunrise/sunset today (only near the poles)
    }
    time_t dayMidnight = midnightUTC;
    if (now >= rise) {
      if (!sunTimes(midnightUTC + 86400, lat, lon, nextRise, unused)) return;
    } else {
      nextRise = rise;
      dayMidnight = midnightUTC - 86400;
      if (!sunTimes(dayMidnight, lat, lon, rise, set)) return;
    }
    tmElements_t day;
    breakTime(dayMidnight, day); // day.Wday: 1 = Sunday
    astro::PlanetaryHour hour = astro::planetaryHour(now, rise, set, nextRise, day.Wday - 1);
    display.drawBitmap(PLANET_HOUR_X, PLANET_HOUR_Y, planet_icons[hour.ruler], PLANET_ICONS_SIZE,
                       PLANET_ICONS_SIZE, GxEPD_BLACK);

    // A very slight buzz when a new planetary hour begins (only when it has just begun,
    // not when the face is redrawn later, e.g. after using the menu)
    if (lastPlanetaryHourStart != 0 && hour.start != lastPlanetaryHourStart &&
        now - hour.start < 120) {
      vibMotor(40, 2); // one 40 ms pulse
    }
    lastPlanetaryHourStart = hour.start;
  }

  static RTC_DATA_ATTR time_t lastPlanetaryHourStart;

  // By day: today's sunrise and sunset, and the sun on the arc (how far through the
  // daylight we are). At night (sunset to sunrise): moonrise and the coming sunrise,
  // and the moon on the arc showing how far through the night we are.
  void drawSun() {
    float lat = settings.lat.toFloat(), lon = settings.lon.toFloat();
    time_t now = nowUTC();
    time_t midnightUTC = utcMidnight(currentTime.Year + 1970, currentTime.Month, currentTime.Day);
    time_t riseUTC, setUTC;
    if (!sunTimes(midnightUTC, lat, lon, riseUTC, setUTC)) {
      // The sun doesn't rise or set today (only near the poles): show no times
      printTimes(nullptr, "--:--", nullptr, "--:--");
      return;
    }

    char line1[8], line2[8];
    if (now >= riseUTC && now < setUTC) {
      // Day: the labels in the background already say "Sunrise" and "Sunset"
      formatLocalTime(riseUTC, line1, sizeof(line1));
      formatLocalTime(setUTC, line2, sizeof(line2));
      printTimes(nullptr, line1, nullptr, line2);
      int step = (int)((now - riseUTC) * SUN_ARC_STEPS / (setUTC - riseUTC));
      display.drawBitmap(sun_arc_positions[step][0], sun_arc_positions[step][1], sun_icon,
                         SUN_WIDTH, SUN_HEIGHT, GxEPD_BLACK);
      return;
    }

    // Night. It runs from the last sunset (today's in the evening, yesterday's before
    // dawn) to the coming sunrise (tomorrow's in the evening, today's before dawn).
    time_t lastSunset = setUTC, nextSunrise = riseUTC, unused;
    bool nightKnown = (now >= setUTC)
                          ? sunTimes(midnightUTC + 86400, lat, lon, nextSunrise, unused)
                          : sunTimes(midnightUTC - 86400, lat, lon, unused, lastSunset);
    if (!nightKnown) {
      nextSunrise = 0;
    }
    updateMoon(now, lat, lon);
    if (moonRiseUTC) {
      formatLocalTime(moonRiseUTC, line1, sizeof(line1));
    } else {
      strcpy(line1, "--:--");
    }
    if (nextSunrise) {
      formatLocalTime(nextSunrise, line2, sizeof(line2));
    } else {
      strcpy(line2, "--:--");
    }
    printTimes("Moonrise", line1, "Sunrise", line2);

    // Third line, night only: when the moon sets after the moonrise shown above (or,
    // on a day with no moonrise, that day's moonset)
    char line3[8] = "--:--";
    if (moonSetUTC) {
      formatLocalTime(moonSetUTC, line3, sizeof(line3));
    }
    replaceLabel("Moonset", LABEL3_LEFT, LABEL3_TOP, LABEL3_RIGHT, LABEL3_BOTTOM);
    display.setCursor(MOONSET_LEFT, MOONSET_BOTTOM + 1);
    printTight(line3);

    // The moon counts down the night: at the left end at sunset, the right end at sunrise
    if (nightKnown && nextSunrise > lastSunset) {
      int step = (int)((now - lastSunset) * SUN_ARC_STEPS / (nextSunrise - lastSunset));
      step = constrain(step, 0, SUN_ARC_STEPS);
      display.drawBitmap(sun_arc_positions[step][0] + MOON_OFFSET_X,
                         sun_arc_positions[step][1] + MOON_OFFSET_Y, moon_icon, MOON_WIDTH,
                         MOON_HEIGHT, GxEPD_BLACK);
    }
  }

  // The two lines under "Steps". A label of nullptr keeps the background's wording;
  // otherwise the background label is blanked and this one is written, right-aligned.
  void printTimes(const char *label1, const char *time1, const char *label2, const char *time2) {
    display.setTextSize(2);
    if (label1) {
      replaceLabel(label1, LABEL1_LEFT, LABEL1_TOP, LABEL1_RIGHT, LABEL1_BOTTOM);
    }
    if (label2) {
      replaceLabel(label2, LABEL2_LEFT, LABEL2_TOP, LABEL2_RIGHT, LABEL2_BOTTOM);
    }
    display.setCursor(SUNRISE_LEFT, SUNRISE_BOTTOM + 1);
    printTight(time1);
    display.setCursor(SUNSET_LEFT, SUNSET_BOTTOM + 1);
    printTight(time2);
  }

  void replaceLabel(const char *label, int16_t left, int16_t top, int16_t right, int16_t bottom) {
    display.fillRect(left, top, right - left + 1, bottom - top + 1, GxEPD_WHITE);
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(label, 0, 0, &x1, &y1, &w, &h);
    display.setCursor(right + 1 - (int16_t)w - x1, bottom + 1);
    display.print(label);
  }

  // Sunrise and sunset (UTC timestamps, to the nearest minute) on the UTC date that
  // starts at midnightUTC. Returns false if the sun doesn't rise and set that day.
  // Results are kept for the last few days used (yesterday, today, tomorrow), in
  // memory that survives deep sleep, so each is only calculated once.
  bool sunTimes(time_t midnightUTC, float lat, float lon, time_t &rise, time_t &set) {
    for (int i = 0; i < SUN_CACHE_DAYS; i++) {
      if (sunCache[i].midnightUTC == midnightUTC) {
        rise = sunCache[i].rise;
        set = sunCache[i].set;
        return sunCache[i].rises;
      }
    }
    tmElements_t day;
    breakTime(midnightUTC, day);
    SunTimes sun = calculateSunTimes(day.Year + 1970, day.Month, day.Day, lat, lon);
    SunCacheEntry &e = sunCache[sunCacheNext];
    sunCacheNext = (sunCacheNext + 1) % SUN_CACHE_DAYS;
    e.midnightUTC = midnightUTC;
    e.rises = sun.rises;
    e.rise = midnightUTC + (time_t)lroundf(sun.sunriseUTC) * 60;
    e.set = midnightUTC + (time_t)lroundf(sun.sunsetUTC) * 60;
    rise = e.rise;
    set = e.set;
    return e.rises;
  }

  struct SunCacheEntry {
    time_t midnightUTC;
    time_t rise, set;
    bool rises;
  };
  static const int SUN_CACHE_DAYS = 4;
  static RTC_DATA_ATTR SunCacheEntry sunCache[SUN_CACHE_DAYS];
  static RTC_DATA_ATTR int sunCacheNext;

  // ---- Battery ----------------------------------------------------------------

  // Bottom-right corner, empty in the design: a battery outline with 4 bars, and
  // "LOW" beside it in low-battery mode
  static const int16_t BATT_X = 176, BATT_Y = 187; // top-left of the battery body
  static RTC_DATA_ATTR int8_t batteryBars;         // bars shown (0-4), -1 = not read yet
  static RTC_DATA_ATTR bool lowBattery;

  // Average of 16 readings (each a few microseconds), in volts
  float readBatteryVoltage() {
    float sum = 0;
    for (int i = 0; i < 16; i++) {
      sum += getBatteryVoltage();
    }
    return sum / 16;
  }

  // Li-Po charge from voltage. The curve isn't a straight line, so interpolate in a
  // table (to be calibrated by logging one full discharge of this watch's battery).
  static int batteryPercent(float v) {
    static const float VOLTS[] = {4.15, 4.05, 3.95, 3.87, 3.80, 3.75, 3.70, 3.65, 3.55, 3.40};
    static const int PERCENT[] = {100, 90, 80, 70, 60, 50, 40, 25, 10, 0};
    if (v >= VOLTS[0]) return 100;
    for (int i = 1; i < 10; i++) {
      if (v >= VOLTS[i]) {
        float f = (v - VOLTS[i]) / (VOLTS[i - 1] - VOLTS[i]);
        return PERCENT[i] + (int)lroundf(f * (PERCENT[i - 1] - PERCENT[i]));
      }
    }
    return 0;
  }

  void drawBattery() {
    float v = readBatteryVoltage();
    int pct = batteryPercent(v);

    // Bars: 4 above 75%, 3 above 50%, 2 above 25%, 1 above 10%. To stop the icon
    // flickering between two bars, it only changes once the charge is 3% past the
    // boundary. Above 4.15 V (charging on USB, or full) always show 4 bars.
    static const int BAR_FROM[] = {0, 10, 25, 50, 75}; // percent at which N bars start
    if (batteryBars < 0 || v > 4.15) {
      batteryBars = pct > 75 ? 4 : pct > 50 ? 3 : pct > 25 ? 2 : pct > 10 ? 1 : 0;
    } else {
      while (batteryBars < 4 && pct >= BAR_FROM[batteryBars + 1] + 3) batteryBars++;
      while (batteryBars > 0 && pct < BAR_FROM[batteryBars] - 3) batteryBars--;
    }

    // Low-battery mode below 3.60 V; leave it again above 3.70 V (the voltage
    // recovers a little when the watch is idle, so a gap stops it switching back and forth)
    if (v < 3.60f) {
      lowBattery = true;
    } else if (v > 3.70f) {
      lowBattery = false;
    }

    // Body 19x10 with a 2x4 terminal on the right; bars 3x6 inside
    display.drawRect(BATT_X, BATT_Y, 19, 10, GxEPD_BLACK);
    display.fillRect(BATT_X + 19, BATT_Y + 3, 2, 4, GxEPD_BLACK);
    for (int i = 0; i < batteryBars; i++) {
      display.fillRect(BATT_X + 2 + i * 4, BATT_Y + 2, 3, 6, GxEPD_BLACK);
    }
    if (lowBattery) {
      display.setTextSize(2);
      int16_t x1, y1;
      uint16_t w, h;
      display.getTextBounds("LOW", 0, 0, &x1, &y1, &w, &h);
      display.setCursor(BATT_X - 4 - (int16_t)w - x1, BATT_Y + 10);
      display.print("LOW");
    }
  }

  time_t utcMidnight(int year, int month, int day) {
    tmElements_t midnight = {};
    midnight.Year = year - 1970;
    midnight.Month = month;
    midnight.Day = day;
    return makeTime(midnight);
  }

  // Which moonrise and moonset to show:
  //  - while the moon is up: when it rose (even if that was yesterday) and when it sets;
  //  - once it has set: the moonrise on the date shown on the watch (--:-- on the
  //    roughly monthly days without one) and the moonset after it.
  // They take a moment to search for, so they are kept between the minute-by-minute
  // wake-ups (in memory that survives deep sleep) and only recalculated when the date
  // changes or the moon rises or sets.
  static RTC_DATA_ATTR long moonForDate;   // date of the last calculation, as YYYYMMDD
  static RTC_DATA_ATTR time_t moonCheckedAt;
  static RTC_DATA_ATTR bool moonWasUp;
  static RTC_DATA_ATTR time_t moonRiseUTC; // the moonrise to show (0 = none that day)
  static RTC_DATA_ATTR time_t moonSetUTC;  // the moonset to show

  void updateMoon(time_t now, float lat, float lon) {
    long date = (currentTime.Year + 1970L) * 10000 + currentTime.Month * 100 + currentTime.Day;
    bool stale = date != moonForDate || now < moonCheckedAt ||
                 (moonWasUp && moonSetUTC && now >= moonSetUTC) ||               // it has set
                 (!moonWasUp && moonRiseUTC > moonCheckedAt && now >= moonRiseUTC); // it has risen
    if (!stale) {
      return;
    }
    moonForDate = date;
    moonCheckedAt = now;
    moonWasUp = moon::isUp(now, lat, lon);
    if (moonWasUp) {
      moonRiseUTC = moon::findEvent(true, now, -1, 30, lat, lon);
      moonSetUTC = moon::findEvent(false, now, +1, 30, lat, lon);
      return;
    }
    // The local day runs from local midnight to the next local midnight (23 or 25 hours
    // long when the clocks change)
    time_t dayStart = localMidnight(currentTime.Year + 1970, currentTime.Month, currentTime.Day);
    time_t dayEnd = localMidnight(currentTime.Year + 1970, currentTime.Month, currentTime.Day + 1);
    int dayHours = (int)((dayEnd - dayStart + 3599) / 3600);
    time_t rise = moon::findEvent(true, dayStart, +1, dayHours, lat, lon);
    moonRiseUTC = (rise && rise < dayEnd) ? rise : 0;
    if (moonRiseUTC) {
      moonSetUTC = moon::findEvent(false, moonRiseUTC, +1, 30, lat, lon);
    } else {
      time_t set = moon::findEvent(false, dayStart, +1, dayHours, lat, lon);
      moonSetUTC = (set && set < dayEnd) ? set : 0;
    }
  }

  // Local midnight at the start of a date, as a UTC timestamp (day may be one past the
  // end of the month; it rolls over to the next month)
  time_t localMidnight(int year, int month, int day) {
    if (useTimezone()) {
      struct tm local = {};
      local.tm_year = year - 1900;
      local.tm_mon = month - 1;
      local.tm_mday = day;
      local.tm_isdst = -1;
      return mktime(&local);
    }
    return utcMidnight(year, month, 1) + (time_t)(day - 1) * 86400 - gmtOffset;
  }

  // Use the UK summer time rule from settings.h for conversions (if one is set)
  bool useTimezone() {
    if (settings.timezone.length() == 0) {
      return false;
    }
    setenv("TZ", settings.timezone.c_str(), 1);
    tzset();
    return true;
  }

  // The watch's current local time as a UTC timestamp
  time_t nowUTC() {
    if (useTimezone()) {
      struct tm local = {};
      local.tm_year = currentTime.Year + 1970 - 1900;
      local.tm_mon = currentTime.Month - 1;
      local.tm_mday = currentTime.Day;
      local.tm_hour = currentTime.Hour;
      local.tm_min = currentTime.Minute;
      local.tm_sec = currentTime.Second;
      local.tm_isdst = -1; // let the rule decide whether it's GMT or BST
      return mktime(&local);
    }
    return makeTime(currentTime) - gmtOffset;
  }

  // Print with digits 1 pixel closer together than the font's normal spacing,
  // as in the design's "00:00" times
  void printTight(const char *text) {
    for (const char *c = text; *c; c++) {
      display.print(*c);
      if (*c >= '0' && *c <= '9') {
        display.setCursor(display.getCursorX() - 1, display.getCursorY());
      }
    }
  }

  // Turn a UTC timestamp into local "HH:MM", using the UK summer time rule from
  // settings.h (or the fixed offset if none is set)
  void formatLocalTime(time_t utc, char *out, size_t size) {
    struct tm local;
    toLocal(utc, local);
    snprintf(out, size, "%02d:%02d", local.tm_hour, local.tm_min);
  }

  // A UTC timestamp as local (UK) date and time
  void toLocal(time_t utc, struct tm &local) {
    if (useTimezone()) {
      localtime_r(&utc, &local);
    } else {
      time_t shifted = utc + gmtOffset;
      gmtime_r(&shifted, &local);
    }
  }

  // Draw text with its horizontal centre at centreX; bottomY is the last pixel
  // row of the digits (one row above the baseline).
  void drawCentred(const char *text, uint8_t scaleX, uint8_t scaleY, int16_t centreX,
                   int16_t bottomY) {
    display.setTextSize(scaleX, scaleY);
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
    display.setCursor(centreX - (int16_t)w / 2 - x1, bottomY + 1);
    display.print(text);
  }
};

RTC_DATA_ATTR time_t WatchFace::lastPlanetaryHourStart = 0;
RTC_DATA_ATTR WatchFace::SunCacheEntry WatchFace::sunCache[WatchFace::SUN_CACHE_DAYS] = {};
RTC_DATA_ATTR int WatchFace::sunCacheNext = 0;
RTC_DATA_ATTR int8_t WatchFace::batteryBars = -1;
RTC_DATA_ATTR bool WatchFace::lowBattery = false;
RTC_DATA_ATTR long WatchFace::moonForDate = 0;
RTC_DATA_ATTR time_t WatchFace::moonCheckedAt = 0;
RTC_DATA_ATTR bool WatchFace::moonWasUp = false;
RTC_DATA_ATTR time_t WatchFace::moonRiseUTC = 0;
RTC_DATA_ATTR time_t WatchFace::moonSetUTC = 0;
RTC_DATA_ATTR int WatchFace::luckyFirst = 0;
RTC_DATA_ATTR int WatchFace::luckyNext = 0;

WatchFace watchy(settings);

void setup() {
  // Run the processor at 80 MHz instead of 240 MHz: each wake-up is mostly spent
  // waiting for the screen, and a slower processor uses less power while it waits
  setCpuFrequencyMhz(80);
  watchy.init();
}

void loop() {}
