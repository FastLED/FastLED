#include "fl/sketch_macros.h"

#if SKETCH_HAS_LOTS_OF_MEMORY

#include "./ColorBoost.h"

#else

void setup() {
    Serial.begin(9600);
}

void loop() {
    Serial.println("Not enough memory");
    delay(1000);
}

#endif
