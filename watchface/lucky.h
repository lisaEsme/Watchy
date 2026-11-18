// Lucky moments: looks things up in the pre-computed table in lucky_windows.h.
// The watch does no astrology for this; the table was worked out on a computer (see
// README.md, "Menu → Lucky"). Header-only, so it can be tested on the host.
#pragma once
#include <time.h>
// The real table, lucky_windows.h, is personal and gitignored (see lucky_windows.example.h)
#if __has_include("lucky_windows.h")
#include "lucky_windows.h"
#else
#include "lucky_windows.example.h"
#endif

namespace lucky {

// Each time in the table is really +/- 2 minutes (the birth time is rounded), so a
// moment counts as upcoming until 2 minutes after it.
const long SLACK = 120;

// A day with a moment scoring this much or more is a "super lucky" day (the standouts:
// about 19 days in the table's year, against about 122 days with any moment)
const uint8_t SUPER_SCORE = 9;

// "Tyche", or "Fortuna" for the Part of Fortune (day and night formulas alike)
inline const char *pointName(uint8_t point) { return point == TYCHE ? "Tyche" : "Fortuna"; }

// Index of the first moment at or after t (LUCKY_MOMENT_COUNT if there is none)
inline int firstAtOrAfter(time_t t) {
  int lo = 0, hi = LUCKY_MOMENT_COUNT;
  while (lo < hi) {
    int mid = (lo + hi) / 2;
    if ((time_t)LUCKY_MOMENTS[mid].utc < t) {
      lo = mid + 1;
    } else {
      hi = mid;
    }
  }
  return lo;
}

// Index of the next moment that hasn't passed yet (LUCKY_MOMENT_COUNT once the table
// has run out)
inline int firstUpcoming(time_t now) { return firstAtOrAfter(now - SLACK); }

// Best score of the moments in [from, to), 0 if there are none
inline uint8_t bestScore(time_t from, time_t to) {
  uint8_t best = 0;
  for (int i = firstAtOrAfter(from); i < LUCKY_MOMENT_COUNT && (time_t)LUCKY_MOMENTS[i].utc < to;
       i++) {
    if (LUCKY_MOMENTS[i].score > best) {
      best = LUCKY_MOMENTS[i].score;
    }
  }
  return best;
}

} // namespace lucky
