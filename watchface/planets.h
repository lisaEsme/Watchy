// Geocentric ecliptic longitudes of the planets, calculated on the watch.
//
// Mercury to Neptune: orbital elements with secular rates (referred to the ecliptic
// and equinox of date, i.e. tropical), plus the main mutual perturbations of Jupiter,
// Saturn and Uranus; Pluto from a fitted series. After Paul Schlyter, "How to compute
// planetary positions" (stjarnhimlen.se). Heliocentric positions are turned into
// geocentric ones using the sun's position, and corrected for light travel time.
// Sun and Moon come from astro.h, which is more precise.
#pragma once
#include <math.h>
#include <time.h>

#include "astro.h"

namespace planets {

enum Body { SUN, MOON, MERCURY, VENUS, MARS, JUPITER, SATURN, URANUS, NEPTUNE, PLUTO, COUNT };

const double D2R = M_PI / 180.0;

struct Vec {
  double x, y, z;
};

// Heliocentric ecliptic position (AU) of planet `body` at d days since 2000 Jan 0.0
// (the epoch these elements use). Mercury..Neptune and Pluto only.
inline Vec heliocentric(Body body, double d) {
  // N: longitude of ascending node, i: inclination, w: argument of perihelion (degrees),
  // a: semi-major axis (AU), e: eccentricity, M: mean anomaly (degrees)
  struct Elements {
    double N0, N1, i0, i1, w0, w1, a0, a1, e0, e1, M0, M1;
  };
  static const Elements EL[] = {
      /* MERCURY */ {48.3313, 3.24587e-5, 7.0047, 5.00e-8, 29.1241, 1.01444e-5, 0.387098, 0, 0.205635, 5.59e-10, 168.6562, 4.0923344368},
      /* VENUS   */ {76.6799, 2.46590e-5, 3.3946, 2.75e-8, 54.8910, 1.38374e-5, 0.723330, 0, 0.006773, -1.302e-9, 48.0052, 1.6021302244},
      /* MARS    */ {49.5574, 2.11081e-5, 1.8497, -1.78e-8, 286.5016, 2.92961e-5, 1.523688, 0, 0.093405, 2.516e-9, 18.6021, 0.5240207766},
      /* JUPITER */ {100.4542, 2.76854e-5, 1.3030, -1.557e-7, 273.8777, 1.64505e-5, 5.20256, 0, 0.048498, 4.469e-9, 19.8950, 0.0830853001},
      /* SATURN  */ {113.6634, 2.38980e-5, 2.4886, -1.081e-7, 339.3939, 2.97661e-5, 9.55475, 0, 0.055546, -9.499e-9, 316.9670, 0.0334442282},
      /* URANUS  */ {74.0005, 1.3978e-5, 0.7733, 1.9e-8, 96.6612, 3.0565e-5, 19.18171, -1.55e-8, 0.047318, 7.45e-9, 142.5905, 0.011725806},
      /* NEPTUNE */ {131.7806, 3.0173e-5, 1.7700, -2.55e-7, 272.8461, -6.027e-6, 30.05826, 3.313e-8, 0.008606, 2.15e-9, 260.2471, 0.005995147},
  };

  double lon, lat, r;
  if (body == PLUTO) {
    double S = (50.03 + 0.033459652 * d) * D2R;
    double P = (238.95 + 0.003968789 * d) * D2R;
    lon = 238.9508 + 0.00400703 * d - 19.799 * sin(P) + 19.848 * cos(P) + 0.897 * sin(2 * P) -
          4.956 * cos(2 * P) + 0.610 * sin(3 * P) + 1.211 * cos(3 * P) - 0.341 * sin(4 * P) -
          0.190 * cos(4 * P) + 0.128 * sin(5 * P) - 0.034 * cos(5 * P) - 0.038 * sin(6 * P) +
          0.031 * cos(6 * P) + 0.020 * sin(S - P) - 0.010 * cos(S - P);
    lat = -3.9082 - 5.453 * sin(P) - 14.975 * cos(P) + 3.527 * sin(2 * P) + 1.673 * cos(2 * P) -
          1.051 * sin(3 * P) + 0.328 * cos(3 * P) + 0.179 * sin(4 * P) - 0.292 * cos(4 * P) +
          0.019 * sin(5 * P) + 0.100 * cos(5 * P) - 0.031 * sin(6 * P) - 0.026 * cos(6 * P) +
          0.011 * cos(S - P);
    r = 40.72 + 6.68 * sin(P) + 6.90 * cos(P) - 1.18 * sin(2 * P) - 0.03 * cos(2 * P) +
        0.15 * sin(3 * P) - 0.14 * cos(3 * P);
  } else {
    const Elements &el = EL[body - MERCURY];
    double N = (el.N0 + el.N1 * d) * D2R, i = (el.i0 + el.i1 * d) * D2R;
    double w = (el.w0 + el.w1 * d) * D2R, a = el.a0 + el.a1 * d, e = el.e0 + el.e1 * d;
    double M = astro::norm360(el.M0 + el.M1 * d) * D2R;
    // Solve Kepler's equation for the eccentric anomaly
    double E = M + e * sin(M) * (1.0 + e * cos(M));
    for (int k = 0; k < 8; k++) {
      E = E - (E - e * sin(E) - M) / (1 - e * cos(E));
    }
    double xv = a * (cos(E) - e), yv = a * sqrt(1.0 - e * e) * sin(E);
    double v = atan2(yv, xv);
    r = sqrt(xv * xv + yv * yv);
    double xh = r * (cos(N) * cos(v + w) - sin(N) * sin(v + w) * cos(i));
    double yh = r * (sin(N) * cos(v + w) + cos(N) * sin(v + w) * cos(i));
    double zh = r * (sin(v + w) * sin(i));
    lon = atan2(yh, xh) / D2R;
    lat = atan2(zh, sqrt(xh * xh + yh * yh)) / D2R;

    // Main perturbations between Jupiter, Saturn and Uranus (degrees)
    double Mj = (19.8950 + 0.0830853001 * d) * D2R, Ms = (316.9670 + 0.0334442282 * d) * D2R;
    double Mu = (142.5905 + 0.011725806 * d) * D2R;
    if (body == JUPITER) {
      lon += -0.332 * sin(2 * Mj - 5 * Ms - 67.6 * D2R) - 0.056 * sin(2 * Mj - 2 * Ms + 21 * D2R) +
             0.042 * sin(3 * Mj - 5 * Ms + 21 * D2R) - 0.036 * sin(Mj - 2 * Ms) +
             0.022 * cos(Mj - Ms) + 0.023 * sin(2 * Mj - 3 * Ms + 52 * D2R) -
             0.016 * sin(Mj - 5 * Ms - 69 * D2R);
    } else if (body == SATURN) {
      lon += 0.812 * sin(2 * Mj - 5 * Ms - 67.6 * D2R) - 0.229 * cos(2 * Mj - 4 * Ms - 2 * D2R) +
             0.119 * sin(Mj - 2 * Ms - 3 * D2R) + 0.046 * sin(2 * Mj - 6 * Ms - 69 * D2R) +
             0.014 * sin(Mj - 3 * Ms + 32 * D2R);
      lat += -0.020 * cos(2 * Mj - 4 * Ms - 2 * D2R) + 0.018 * sin(2 * Mj - 6 * Ms - 49 * D2R);
    } else if (body == URANUS) {
      lon += 0.040 * sin(Ms - 2 * Mu + 6 * D2R) + 0.035 * sin(Ms - 3 * Mu + 33 * D2R) -
             0.015 * sin(Mj - Mu + 20 * D2R);
    }
  }
  return {r * cos(lat * D2R) * cos(lon * D2R), r * cos(lat * D2R) * sin(lon * D2R), r * sin(lat * D2R)};
}

// The sun as seen from the Earth (AU), same elements and epoch as above
inline Vec sunGeocentric(double d) {
  double w = (282.9404 + 4.70935e-5 * d) * D2R, e = 0.016709 - 1.151e-9 * d;
  double M = astro::norm360(356.0470 + 0.9856002585 * d) * D2R;
  double E = M + e * sin(M) * (1.0 + e * cos(M));
  double xv = cos(E) - e, yv = sqrt(1.0 - e * e) * sin(E);
  double v = atan2(yv, xv), r = sqrt(xv * xv + yv * yv);
  return {r * cos(v + w), r * sin(v + w), 0};
}

// Apparent geocentric ecliptic longitude (degrees, tropical) of any body at time t
inline double longitude(Body body, time_t t) {
  if (body == SUN) return astro::sunLongitude(t);
  if (body == MOON) return astro::moonLongitude(t);
  double d = (t + 69.0) / 86400.0 + 2440587.5 - 2451543.5; // TT, days since 2000 Jan 0.0
  Vec s = sunGeocentric(d);
  // Correct for light travel time: see the planet where it was when the light left it
  double lt = 0;
  Vec g;
  for (int k = 0; k < 2; k++) {
    Vec p = heliocentric(body, d - lt);
    g = {p.x + s.x, p.y + s.y, p.z + s.z};
    lt = sqrt(g.x * g.x + g.y * g.y + g.z * g.z) * 0.0057755183; // days per AU
  }
  // Annual aberration (about 20 arcseconds) and nutation, to match "apparent" positions
  double lon = atan2(g.y, g.x) / D2R;
  double sunLon = atan2(s.y, s.x) / D2R;
  lon += -20.4898 / 3600.0 * cos((sunLon - lon) * D2R);
  return astro::norm360(lon + astro::nutationLongitude((d - 1.5) / 36525.0));
}

// True if the body is retrograde (moving backwards through the zodiac) at time t
inline bool retrograde(Body body, time_t t) {
  if (body == SUN || body == MOON) return false;
  double a = longitude(body, t - 43200), b = longitude(body, t + 43200);
  double change = fmod(b - a + 540.0, 360.0) - 180.0;
  return change < 0;
}

} // namespace planets
