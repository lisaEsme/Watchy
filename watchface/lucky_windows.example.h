// lucky_windows.example.h - stand-in for lucky_windows.h, which holds the real table.
//
// The real table is personal (worked out from a birth chart), and this repository is
// public, so lucky_windows.h is gitignored like wifi_secrets.h. To use it:
//  - Building on your computer: put lucky_windows.h in this folder.
//  - Building with GitHub Actions: store the file in the repository secret
//    LUCKY_WINDOWS_H_GZ (gzip -9c lucky_windows.h | base64 -w0); the workflow
//    unpacks it before building.
// Without it, the watch builds with this empty table and the Lucky page says so.
#pragma once
#include <stdint.h>

#define LUCKY_TABLE_MISSING 1

enum LuckPoint : uint8_t { POF_DAY = 0, POF_NIGHT = 1, TYCHE = 2 };
enum LuckAngle : uint8_t { ANG_ASC = 0, ANG_MC = 1, ANG_DSC = 2, ANG_IC = 3 };

struct LuckyMoment { uint32_t utc; uint8_t point; uint8_t angle; uint8_t score; };

// One placeholder entry (C++ has no empty arrays); the count of 0 means no moments
static const LuckyMoment LUCKY_MOMENTS[] = {{0u, 0, 0, 0}};
static const uint16_t LUCKY_MOMENT_COUNT = 0;
