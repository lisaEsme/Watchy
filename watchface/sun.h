// Sunrise and sunset times, calculated on the watch (no internet needed).
//
// Uses the method of NOAA's Solar Calculator (based on Jean Meeus, "Astronomical
// Algorithms"): https://gml.noaa.gov/grad/solcalc/calcdetails.html
// Accurate to within about a minute. "Sunrise" and "sunset" are when the top of
// the sun touches the horizon, allowing for atmospheric refraction (zenith 90.833 degrees).
#pragma once
#include <math.h>

struct SunTimes {
  bool rises;       // false on days the sun never rises or never sets (polar regions)
  float sunriseUTC; // minutes after midnight UTC on the given date
  float sunsetUTC;  // minutes after midnight UTC on the given date
};

// Julian Day at 00:00 UTC on a calendar date (Gregorian)
inline double julianDayAtMidnight(int year, int month, int day) {
  int a = (14 - month) / 12;
  long y = year + 4800 - a;
  int m = month + 12 * a - 3;
  long jdn = day + (153 * m + 2) / 5 + 365 * y + y / 4 - y / 100 + y / 400 - 32045;
  return jdn - 0.5;
}

// Time of sunrise (rising = true) or sunset, in minutes after midnight UTC, using the
// sun's position at minuteUTC. Returns NAN if the sun doesn't rise/set that day.
inline double sunEventUTC(bool rising, double jdMidnight, double minuteUTC, double latitude,
                          double longitude) {
  const double D2R = M_PI / 180.0, R2D = 180.0 / M_PI;
  double jc = (jdMidnight + minuteUTC / 1440.0 - 2451545.0) / 36525.0; // Julian century

  double meanLong = fmod(280.46646 + jc * (36000.76983 + jc * 0.0003032), 360.0);
  double meanAnom = 357.52911 + jc * (35999.05029 - 0.0001537 * jc);
  double eccent = 0.016708634 - jc * (0.000042037 + 0.0000001267 * jc);
  double centre = sin(meanAnom * D2R) * (1.914602 - jc * (0.004817 + 0.000014 * jc)) +
                  sin(2 * meanAnom * D2R) * (0.019993 - 0.000101 * jc) +
                  sin(3 * meanAnom * D2R) * 0.000289;
  double omega = 125.04 - 1934.136 * jc;
  double apparentLong = meanLong + centre - 0.00569 - 0.00478 * sin(omega * D2R);
  double meanObliq = 23.0 + (26.0 + (21.448 - jc * (46.815 + jc * (0.00059 - jc * 0.001813))) / 60.0) / 60.0;
  double obliq = meanObliq + 0.00256 * cos(omega * D2R);
  double decl = asin(sin(obliq * D2R) * sin(apparentLong * D2R)); // radians

  double y = tan(obliq * D2R / 2) * tan(obliq * D2R / 2);
  double eqTime = 4.0 * R2D *
                  (y * sin(2 * meanLong * D2R) - 2 * eccent * sin(meanAnom * D2R) +
                   4 * eccent * y * sin(meanAnom * D2R) * cos(2 * meanLong * D2R) -
                   0.5 * y * y * sin(4 * meanLong * D2R) - 1.25 * eccent * eccent * sin(2 * meanAnom * D2R));

  double lat = latitude * D2R;
  double cosHourAngle = cos(90.833 * D2R) / (cos(lat) * cos(decl)) - tan(lat) * tan(decl);
  if (cosHourAngle < -1.0 || cosHourAngle > 1.0) {
    return NAN; // midnight sun or polar night
  }
  double hourAngleDeg = acos(cosHourAngle) * R2D;
  double solarNoon = 720.0 - 4.0 * longitude - eqTime;
  return rising ? solarNoon - 4.0 * hourAngleDeg : solarNoon + 4.0 * hourAngleDeg;
}

// year e.g. 2026, month 1-12, day 1-31; latitude north positive, longitude east positive
inline SunTimes calculateSunTimes(int year, int month, int day, float latitude, float longitude) {
  double jd = julianDayAtMidnight(year, month, day);
  SunTimes t = {false, 0, 0};
  // Start from midday, then refine using the sun's position at each estimated time
  double rise = 720.0, set = 720.0;
  for (int i = 0; i < 3; i++) {
    rise = sunEventUTC(true, jd, rise, latitude, longitude);
    set = sunEventUTC(false, jd, set, latitude, longitude);
    if (isnan(rise) || isnan(set)) {
      return t;
    }
  }
  t.rises = true;
  t.sunriseUTC = (float)rise;
  t.sunsetUTC = (float)set;
  return t;
}
