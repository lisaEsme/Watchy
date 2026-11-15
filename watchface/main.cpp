// Our own watch face. Starts as a copy of examples/WatchFaces/Basic.
// Build and upload with PlatformIO (see platformio.ini in the repository root).
#include <Arduino.h>
#include <Watchy.h>
#include "settings.h"

Watchy watchy(settings);

void setup() {
  watchy.init();
}

void loop() {}
