
#include <Arduino.h>
#include <FastLED.h>


#if !SKETCH_HAS_LOTS_OF_MEMORY
// Platform does not have enough memory
void setup() {}
void loop() {}
#else

#include "Downscale.h"

#endif  // SKETCH_HAS_LOTS_OF_MEMORY
