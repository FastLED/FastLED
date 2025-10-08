// FastLED.h must be included first to trigger precompiled headers for FastLED's build system
#include "FastLED.h"

#include "fl/sketch_macros.h"

#if SKETCH_HAS_LOTS_OF_MEMORY

#include "./Fire2023.h"

#else

void setup() {
    Serial.begin(9600);
}

void loop() {
    Serial.println("Not enough memory");
    delay(1000);
}
#endif
