// Astrological positions, calculated on the watch (no internet needed).
//
//  - Moon: apparent geocentric ecliptic longitude from Jean Meeus, "Astronomical
//    Algorithms" (2nd ed.), chapter 47 (the ELP-2000/82 main terms; accurate to about
//    10 arcseconds), plus nutation in longitude (chapter 22, short form).
//  - Sun: apparent geocentric ecliptic longitude, Meeus chapter 25 (about 0.01 degrees).
//  - Signs are tropical: 0 = Aries ... 11 = Pisces, each 30 degrees of longitude.
//  - Moon phase from the moon's elongation from the sun.
//  - Planetary hours: traditional (unequal hours; Chaldean order).
// Times are Unix timestamps (seconds since 1970, UTC).
#pragma once
#include <math.h>
#include <time.h>

namespace astro {

const double D2R = M_PI / 180.0;

inline double norm360(double a) {
  a = fmod(a, 360.0);
  return a < 0 ? a + 360.0 : a;
}

// Julian centuries since J2000.0 in Terrestrial Time (TT is about 69 seconds ahead of
// UTC in the 2020s; the moon moves about 35 arcseconds in that time)
inline double centuriesTT(time_t t) {
  const double DELTA_T = 69.0;
  return ((t + DELTA_T) / 86400.0 + 2440587.5 - 2451545.0) / 36525.0;
}

// Nutation in longitude (degrees), Meeus chapter 22, accurate to 0.5 arcseconds
inline double nutationLongitude(double T) {
  double omega = (125.04452 - 1934.136261 * T) * D2R;
  double Ls = (280.4665 + 36000.7698 * T) * D2R;
  double Lm = (218.3165 + 481267.8813 * T) * D2R;
  return (-17.20 * sin(omega) - 1.32 * sin(2 * Ls) - 0.23 * sin(2 * Lm) + 0.21 * sin(2 * omega)) / 3600.0;
}

// Apparent ecliptic longitude of the sun (degrees), Meeus chapter 25
inline double sunLongitude(time_t t) {
  double T = centuriesTT(t);
  double L0 = 280.46646 + 36000.76983 * T + 0.0003032 * T * T;
  double M = (357.52911 + 35999.05029 * T - 0.0001537 * T * T) * D2R;
  double C = (1.914602 - 0.004817 * T - 0.000014 * T * T) * sin(M) +
             (0.019993 - 0.000101 * T) * sin(2 * M) + 0.000289 * sin(3 * M);
  double omega = (125.04 - 1934.136 * T) * D2R;
  return norm360(L0 + C - 0.00569 - 0.00478 * sin(omega));
}

// Apparent ecliptic longitude of the moon (degrees), Meeus chapter 47
inline double moonLongitude(time_t t) {
  // Periodic terms for longitude: multiples of D, M, M', F and the coefficient
  // (millionths of a degree). Meeus table 47.A.
  static const signed char ARG[][4] = {
      {0, 0, 1, 0},  {2, 0, -1, 0}, {2, 0, 0, 0},   {0, 0, 2, 0},  {0, 1, 0, 0},  {0, 0, 0, 2},
      {2, 0, -2, 0}, {2, -1, -1, 0}, {2, 0, 1, 0},  {2, -1, 0, 0}, {0, 1, -1, 0}, {1, 0, 0, 0},
      {0, 1, 1, 0},  {2, 0, 0, -2}, {0, 0, 1, 2},   {0, 0, 1, -2}, {4, 0, -1, 0}, {0, 0, 3, 0},
      {4, 0, -2, 0}, {2, 1, -1, 0}, {2, 1, 0, 0},   {1, 0, -1, 0}, {1, 1, 0, 0},  {2, -1, 1, 0},
      {2, 0, 2, 0},  {4, 0, 0, 0},  {2, 0, -3, 0},  {0, 1, -2, 0}, {2, 0, -1, 2}, {2, -1, -2, 0},
      {1, 0, 1, 0},  {2, -2, 0, 0}, {0, 1, 2, 0},   {0, 2, 0, 0},  {2, -2, -1, 0}, {2, 0, 1, -2},
      {2, 0, 0, 2},  {4, -1, -1, 0}, {0, 0, 2, 2},  {3, 0, -1, 0}, {2, 1, 1, 0},  {4, -1, -2, 0},
      {0, 2, -1, 0}, {2, 2, -1, 0}, {2, 1, -2, 0},  {2, -1, 0, -2}, {4, 0, 1, 0}, {0, 0, 4, 0},
      {4, -1, 0, 0}, {1, 0, -2, 0}, {2, 1, 0, -2},  {0, 0, 2, -2}, {1, 1, 1, 0},  {3, 0, -2, 0},
      {4, 0, -3, 0}, {2, -1, 2, 0}, {0, 2, 1, 0},   {1, 1, -1, 0}, {2, 0, 3, 0}};
  static const long COEF[] = {
      6288774, 1274027, 658314, 213618, -185116, -114332, 58793, 57066, 53322, 45758,
      -40923,  -34720,  -30383, 15327,  -12528,  10980,   10675, 10034, 8548,  -7888,
      -6766,   -5163,   4987,   4036,   3994,    3861,    3665,  -2689, -2602, 2390,
      -2348,   2236,    -2120,  -2069,  2048,    -1773,   -1595, 1215,  -1110, -892,
      -810,    759,     -713,   -700,   691,     596,     549,   537,   520,   -487,
      -399,    -381,    351,    -340,   330,     327,     -323,  299,   294};
  const int N = sizeof(COEF) / sizeof(COEF[0]);

  double T = centuriesTT(t), T2 = T * T, T3 = T2 * T, T4 = T3 * T;
  double Lp = 218.3164477 + 481267.88123421 * T - 0.0015786 * T2 + T3 / 538841.0 - T4 / 65194000.0;
  double D = 297.8501921 + 445267.1114034 * T - 0.0018819 * T2 + T3 / 545868.0 - T4 / 113065000.0;
  double M = 357.5291092 + 35999.0502909 * T - 0.0001536 * T2 + T3 / 24490000.0;
  double Mp = 134.9633964 + 477198.8675055 * T + 0.0087414 * T2 + T3 / 69699.0 - T4 / 14712000.0;
  double F = 93.2720950 + 483202.0175233 * T - 0.0036539 * T2 - T3 / 3526000.0 + T4 / 863310000.0;
  double A1 = 119.75 + 131.849 * T, A2 = 53.09 + 479264.290 * T;
  double E = 1.0 - 0.002516 * T - 0.0000074 * T2; // Earth's orbital eccentricity factor

  double sum = 0;
  for (int i = 0; i < N; i++) {
    double arg = (ARG[i][0] * D + ARG[i][1] * M + ARG[i][2] * Mp + ARG[i][3] * F) * D2R;
    double c = COEF[i];
    int m = ARG[i][1] < 0 ? -ARG[i][1] : ARG[i][1];
    if (m == 1) {
      c *= E;
    } else if (m == 2) {
      c *= E * E;
    }
    sum += c * sin(arg);
  }
  sum += 3958 * sin(A1 * D2R) + 1962 * sin((Lp - F) * D2R) + 318 * sin(A2 * D2R);
  return norm360(Lp + sum / 1e6 + nutationLongitude(T));
}

// Tropical sign of an ecliptic longitude: 0 = Aries, 1 = Taurus, ... 11 = Pisces
inline int sign(double longitude) { return (int)(norm360(longitude) / 30.0) % 12; }

// How far through its cycle the moon is: 0 = new, 0.25 = first quarter, 0.5 = full,
// 0.75 = last quarter (from the moon's elongation east of the sun)
inline double moonAge(time_t t) { return norm360(moonLongitude(t) - sunLongitude(t)) / 360.0; }

// Fraction of the moon's disc that is lit (0 = new, 1 = full)
inline double moonIllumination(time_t t) {
  return (1.0 - cos(moonAge(t) * 2.0 * M_PI)) / 2.0;
}

// ---- Planetary hours ----------------------------------------------------------

// The seven traditional planets in Chaldean order
enum Planet { SATURN, JUPITER, MARS, SUN, VENUS, MERCURY, MOON };

// Ruler of a planetary day, by day of the week (0 = Sunday ... 6 = Saturday)
inline Planet dayRuler(int weekday) {
  static const Planet RULERS[] = {SUN, MOON, MARS, MERCURY, JUPITER, VENUS, SATURN};
  return RULERS[weekday % 7];
}

struct PlanetaryHour {
  Planet ruler;   // planet ruling the current hour
  Planet day;     // planet ruling the planetary day (which began at sunrise)
  int number;     // 0-11 = hours of the day (from sunrise), 12-23 = hours of the night
  time_t start;   // when this hour began
  time_t end;     // when it ends
};

// sunrise/sunset/nextSunrise: the planetary day containing `now`, i.e. the most recent
// sunrise at or before now, that day's sunset, and the following sunrise. weekday is
// the local day of the week of that sunrise (0 = Sunday).
inline PlanetaryHour planetaryHour(time_t now, time_t sunrise, time_t sunset,
                                   time_t nextSunrise, int weekday) {
  PlanetaryHour h;
  h.day = dayRuler(weekday);
  if (now < sunset) { // daytime: 12 equal hours from sunrise to sunset
    double len = (sunset - sunrise) / 12.0;
    h.number = (int)((now - sunrise) / len);
    if (h.number > 11) h.number = 11;
    h.start = sunrise + (time_t)(h.number * len);
    h.end = sunrise + (time_t)((h.number + 1) * len);
  } else { // night: 12 equal hours from sunset to the next sunrise
    double len = (nextSunrise - sunset) / 12.0;
    int n = (int)((now - sunset) / len);
    if (n > 11) n = 11;
    h.number = 12 + n;
    h.start = sunset + (time_t)(n * len);
    h.end = sunset + (time_t)((n + 1) * len);
  }
  // Each hour's ruler follows the Chaldean order, starting from the day's ruler
  h.ruler = (Planet)(((int)h.day + h.number) % 7);
  return h;
}

} // namespace astro
