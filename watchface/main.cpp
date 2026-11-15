// Our own watch face, designed in watchface/design/face.png.
// Build and upload with PlatformIO (see platformio.ini in the repository root).
//
// Live now: time (24-hour), date and step count. By day, today's sunrise and sunset,
// with the sun travelling along the arc; at night, moonrise and the coming sunrise,
// with the moon travelling along the arc while it is up.
// Static for now (drawn from the design): moon phase, planetary hour and moon sign.
// See design/face-annotated.png.
#include <Arduino.h>
#include <Watchy.h>

#include "settings.h"
#include "PocketPixel.h"
#include "face_background.h"
#include "face_layout.h"
#include "face_sun.h"
#include "moon.h"
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

    display.setTextSize(1); // the menus share this display, so leave it at normal size
  }

private:
  // By day: today's sunrise and sunset, and the sun on the arc (how far through the
  // daylight we are). At night (sunset to sunrise): moonrise and the coming sunrise,
  // and the moon on the arc while it is up (how far from moonrise to moonset).
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

    // Night. The coming sunrise is today's if it's before dawn, otherwise tomorrow's.
    time_t nextSunrise = riseUTC, unused;
    if (now >= setUTC && !sunTimes(midnightUTC + 86400, lat, lon, nextSunrise, unused)) {
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

    if (moonIsUp && moonRiseUTC && moonSetUTC > moonRiseUTC) {
      int step = (int)((now - moonRiseUTC) * SUN_ARC_STEPS / (moonSetUTC - moonRiseUTC));
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
  bool sunTimes(time_t midnightUTC, float lat, float lon, time_t &rise, time_t &set) {
    tmElements_t day;
    breakTime(midnightUTC, day);
    SunTimes sun = calculateSunTimes(day.Year + 1970, day.Month, day.Day, lat, lon);
    if (!sun.rises) {
      return false;
    }
    rise = midnightUTC + (time_t)lroundf(sun.sunriseUTC) * 60;
    set = midnightUTC + (time_t)lroundf(sun.sunsetUTC) * 60;
    return true;
  }

  time_t utcMidnight(int year, int month, int day) {
    tmElements_t midnight = {};
    midnight.Year = year - 1970;
    midnight.Month = month;
    midnight.Day = day;
    return makeTime(midnight);
  }

  // Moonrise/moonset take a moment to search for, so they are kept between the
  // minute-by-minute wake-ups (in memory that survives deep sleep) and only
  // recalculated hourly, or as soon as the moon rises or sets.
  static RTC_DATA_ATTR time_t moonCheckedAt;
  static RTC_DATA_ATTR bool moonIsUp;
  static RTC_DATA_ATTR time_t moonRiseUTC; // when it rose (if up) or will next rise
  static RTC_DATA_ATTR time_t moonSetUTC;  // when it will set (if up)

  void updateMoon(time_t now, float lat, float lon) {
    bool stale = now < moonCheckedAt || now - moonCheckedAt >= 3600 ||
                 (moonIsUp && moonSetUTC && now >= moonSetUTC) ||
                 (!moonIsUp && moonRiseUTC && now >= moonRiseUTC);
    if (!stale) {
      return;
    }
    moonCheckedAt = now;
    moonIsUp = moon::isUp(now, lat, lon);
    if (moonIsUp) {
      moonRiseUTC = moon::findEvent(true, now, -1, 30, lat, lon);
      moonSetUTC = moon::findEvent(false, now, +1, 30, lat, lon);
    } else {
      moonRiseUTC = moon::findEvent(true, now, +1, 30, lat, lon);
      moonSetUTC = 0;
    }
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
    if (useTimezone()) {
      localtime_r(&utc, &local);
    } else {
      time_t shifted = utc + gmtOffset;
      gmtime_r(&shifted, &local);
    }
    snprintf(out, size, "%02d:%02d", local.tm_hour, local.tm_min);
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

RTC_DATA_ATTR time_t WatchFace::moonCheckedAt = 0;
RTC_DATA_ATTR bool WatchFace::moonIsUp = false;
RTC_DATA_ATTR time_t WatchFace::moonRiseUTC = 0;
RTC_DATA_ATTR time_t WatchFace::moonSetUTC = 0;

WatchFace watchy(settings);

void setup() {
  watchy.init();
}

void loop() {}
