// Our own watch face, designed in watchface/design/face.png.
// Build and upload with PlatformIO (see platformio.ini in the repository root).
//
// Live now: time (24-hour), date, step count, today's sunrise and sunset, and the
// sun travelling along the arc from sunrise to sunset.
// Static for now (drawn from the design): moon phase, planetary hour and moon sign.
// See design/face-annotated.png.
#include <Arduino.h>
#include <Watchy.h>

#include "settings.h"
#include "PocketPixel.h"
#include "face_background.h"
#include "face_layout.h"
#include "face_sun.h"
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
  // Today's sunrise and sunset for our location (settings.h), in local time, and the
  // sun's position along the arc: how far through the daylight hours we are
  void drawSun() {
    int year = currentTime.Year + 1970;
    SunTimes sun = calculateSunTimes(year, currentTime.Month, currentTime.Day,
                                     settings.lat.toFloat(), settings.lon.toFloat());
    char rise[8] = "--:--", set[8] = "--:--";
    int step = 0; // 0 = sunrise end of the arc, SUN_ARC_STEPS = sunset end
    if (sun.rises) {
      tmElements_t midnight = {};
      midnight.Year = currentTime.Year;
      midnight.Month = currentTime.Month;
      midnight.Day = currentTime.Day;
      time_t midnightUTC = makeTime(midnight);
      time_t riseUTC = midnightUTC + (time_t)lroundf(sun.sunriseUTC) * 60; // nearest minute
      time_t setUTC = midnightUTC + (time_t)lroundf(sun.sunsetUTC) * 60;
      formatLocalTime(riseUTC, rise, sizeof(rise));
      formatLocalTime(setUTC, set, sizeof(set));

      // Before sunrise the sun waits at the start of the arc, after sunset at the end
      time_t now = nowUTC();
      if (now >= setUTC) {
        step = SUN_ARC_STEPS;
      } else if (now > riseUTC) {
        step = (int)((now - riseUTC) * SUN_ARC_STEPS / (setUTC - riseUTC));
      }
    }

    display.setTextSize(2);
    display.setCursor(SUNRISE_LEFT, SUNRISE_BOTTOM + 1);
    printTight(rise);
    display.setCursor(SUNSET_LEFT, SUNSET_BOTTOM + 1);
    printTight(set);

    display.drawBitmap(sun_arc_positions[step][0], sun_arc_positions[step][1], sun_icon,
                       SUN_WIDTH, SUN_HEIGHT, GxEPD_BLACK);
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

WatchFace watchy(settings);

void setup() {
  watchy.init();
}

void loop() {}
