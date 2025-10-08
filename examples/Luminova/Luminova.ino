// FastLED.h must be included first to trigger precompiled headers for FastLED's build system
#include "FastLED.h"

#include "fl/sketch_macros.h"

#if SKETCH_HAS_LOTS_OF_MEMORY
#include "Luminova.h"
#else
void setup(){}
void loop(){}

#endif
