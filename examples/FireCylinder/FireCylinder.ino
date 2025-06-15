#include "FastLED.h"     // Main FastLED library for controlling LEDs

#if !SKETCH_HAS_LOTS_OF_MEMORY
// Platform does not have enough memory
void setup() {}
void loop() {}
#else
#include "FireCylinder.h"
#endif
