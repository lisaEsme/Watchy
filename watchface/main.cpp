// Our own watch face, designed in watchface/design/face.png.
// Build and upload with PlatformIO (see platformio.ini in the repository root).
//
// Live now: time (24-hour), date and step count, drawn in Pocket Pixel.
// Static for now (drawn from the design): sun arc, moon phase, sunrise and
// moonrise times, planetary hour and moon sign. See design/face-annotated.png.
#include <Arduino.h>
#include <Watchy.h>

#include "settings.h"
#include "PocketPixel.h"
#include "face_background.h"
#include "face_layout.h"
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

    drawSunrise();

    display.setTextSize(1); // the menus share this display, so leave it at normal size
  }

private:
  // Today's sunrise for our location (settings.h), in local time, right-aligned
  // after the "Sunrise" label
  void drawSunrise() {
    int year = currentTime.Year + 1970;
    SunTimes sun = calculateSunTimes(year, currentTime.Month, currentTime.Day,
                                     settings.lat.toFloat(), settings.lon.toFloat());
    char text[8];
    if (sun.rises) {
      formatLocalTime(year, currentTime.Month, currentTime.Day, sun.sunriseUTC, text, sizeof(text));
    } else {
      strcpy(text, "--:--");
    }
    display.setTextSize(2);
    display.setCursor(SUNRISE_LEFT, SUNRISE_BOTTOM + 1);
    printTight(text);
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

  // Turn a time in minutes after midnight UTC on the given date into local "HH:MM",
  // using the UK summer time rule from settings.h (or the fixed offset if none is set)
  void formatLocalTime(int year, int month, int day, float minutesUTC, char *out, size_t size) {
    tmElements_t midnight = {};
    midnight.Year = year - 1970;
    midnight.Month = month;
    midnight.Day = day;
    time_t utc = makeTime(midnight) + (time_t)lroundf(minutesUTC) * 60; // nearest minute
    struct tm local;
    if (settings.timezone.length() > 0) {
      setenv("TZ", settings.timezone.c_str(), 1);
      tzset();
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
