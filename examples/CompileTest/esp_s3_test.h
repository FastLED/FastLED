#pragma once

#include "FastLED.h"

void esp32_s3_tests() {
#if FASTLED_USE_PROGMEM == 0
#error "FASTLED_USE_PROGMEM is 0 for esap32-s3"
#endif

#if SKETCH_HAS_LOTS_OF_MEMORY == 0
#error "SKETCH_HAS_LOTS_OF_MEMORY is 0 for esp32-s3"
#endif
}