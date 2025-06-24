#pragma once

#include "FastLED.h"

void esp_8266_tests() {
#if FASTLED_USE_PROGMEM != 1
#error "FASTLED_USE_PROGMEM is not 1 for AVR"
#endif

#if SKETCH_HAS_LOTS_OF_MEMORY != 0
#error "SKETCH_HAS_LOTS_OF_MEMORY is not 0 for AVR"
#endif
}