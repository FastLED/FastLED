#pragma once

#include "FastLED.h"

void esp32_s3_tests() {
#if FASTLED_USE_PROGMEM != 0
#error "FASTLED_USE_PROGMEM prog memory is turned on for esap32-s3"
#endif

#if SKETCH_HAS_LOTS_OF_MEMORY == 0
#error "SKETCH_HAS_LOTS_OF_MEMORY should have lots of memory for esp32-s3"
#endif
}