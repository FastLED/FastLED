#include "FastLED.h"


#include "fl/sketch_macros.h"

#if !SKETCH_HAS_LOTS_OF_MEMORY
// Platform does not have enough memory
void setup() {}
void loop() {}
#else

#include "curr.h"

#endif
