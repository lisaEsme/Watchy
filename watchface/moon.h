// The moon's position in the sky, and moonrise and moonset times, calculated on the
// watch (no internet needed).
//
// Moon position: the low-precision formulae of the Astronomical Almanac (accurate to
// about 0.3 degrees), as given in e.g. Meeus, "Astronomical Algorithms", and
// https://celestialprogramming.com/. Rise and set are when the moon's upper edge
// touches the horizon, allowing for refraction and the moon's parallax.
// All times are Unix timestamps (seconds since 1970, UTC).
#pragma once
#include <math.h>
#include <time.h>

namespace moon {

const double D2R = M_PI / 180.0;

inline double julianDay(time_t t) { return t / 86400.0 + 2440587.5; }

// Height of the moon's upper edge above the horizon, in degrees, at time t, as seen
// from latitude/longitude (degrees, north and east positive). Positive = visible.
inline double altitude(time_t t, double latitude, double longitude) {
  double jd = julianDay(t);
  double T = (jd - 2451545.0) / 36525.0;

  // Ecliptic longitude and latitude, and horizontal parallax (degrees)
  double lambda = 218.32 + 481267.881 * T + 6.29 * sin((135.0 + 477198.87 * T) * D2R) -
                  1.27 * sin((259.3 - 413335.36 * T) * D2R) + 0.66 * sin((235.7 + 890534.22 * T) * D2R) +
                  0.21 * sin((269.9 + 954397.74 * T) * D2R) - 0.19 * sin((357.5 + 35999.05 * T) * D2R) -
                  0.11 * sin((186.5 + 966404.03 * T) * D2R);
  double beta = 5.13 * sin((93.3 + 483202.02 * T) * D2R) + 0.28 * sin((228.2 + 960400.89 * T) * D2R) -
                0.28 * sin((318.3 + 6003.15 * T) * D2R) - 0.17 * sin((217.6 - 407332.21 * T) * D2R);
  double parallax = 0.9508 + 0.0518 * cos((135.0 + 477198.87 * T) * D2R) +
                    0.0095 * cos((259.3 - 413335.36 * T) * D2R) +
                    0.0078 * cos((235.7 + 890534.22 * T) * D2R) +
                    0.0028 * cos((269.9 + 954397.74 * T) * D2R);

  // Ecliptic -> equatorial (right ascension and declination)
  double eps = (23.439 - 0.0130 * T) * D2R;
  double l = lambda * D2R, b = beta * D2R;
  double x = cos(b) * cos(l);
  double y = cos(eps) * cos(b) * sin(l) - sin(eps) * sin(b);
  double z = sin(eps) * cos(b) * sin(l) + cos(eps) * sin(b);
  double ra = atan2(y, x);
  double dec = asin(z);

  // Hour angle from local sidereal time, then altitude of the moon's centre
  double gmst = fmod(280.46061837 + 360.98564736629 * (jd - 2451545.0), 360.0);
  double hourAngle = (gmst + longitude) * D2R - ra;
  double lat = latitude * D2R;
  double alt = asin(sin(lat) * sin(dec) + cos(lat) * cos(dec) * cos(hourAngle)) / D2R;

  // Standard altitude for rise/set: 0.7275 x parallax - 0.5667 degrees (refraction and
  // semi-diameter), so the result is 0 exactly at moonrise and moonset
  return alt - (0.7275 * parallax - 0.5667);
}

inline bool isUp(time_t t, double latitude, double longitude) {
  return altitude(t, latitude, longitude) > 0;
}

// Find when the moon next rises (rising = true) or sets, searching from `from` in
// direction `dir` (+1 forward, -1 back) for up to `hours`. Returns 0 if none found.
inline time_t findEvent(bool rising, time_t from, int dir, int hours, double latitude,
                        double longitude) {
  const int STEP = 10 * 60; // the moon changes height by at most ~3 degrees in 10 minutes
  time_t a = from;
  double altA = altitude(a, latitude, longitude);
  for (int i = 0; i < hours * 6; i++) {
    time_t b = a + dir * STEP;
    double altB = altitude(b, latitude, longitude);
    // in time order, a rise goes from below to above the horizon
    time_t early = dir > 0 ? a : b, late = dir > 0 ? b : a;
    double altEarly = dir > 0 ? altA : altB, altLate = dir > 0 ? altB : altA;
    if ((rising && altEarly <= 0 && altLate > 0) || (!rising && altEarly > 0 && altLate <= 0)) {
      // narrow down to the second by halving the interval
      while (late - early > 1) {
        time_t mid = early + (late - early) / 2;
        bool midUp = altitude(mid, latitude, longitude) > 0;
        if (midUp == rising) {
          late = mid;
        } else {
          early = mid;
        }
      }
      return late;
    }
    a = b;
    altA = altB;
  }
  return 0;
}

} // namespace moon
