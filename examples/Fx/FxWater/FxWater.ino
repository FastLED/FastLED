// FastLED.h must be included first to trigger precompiled headers for FastLED's build system
#include "FastLED.h"

#include "fl/sketch_macros.h"
#if SKETCH_HAS_LOTS_OF_MEMORY
#include "./FxWater.h"
#else
#include "platforms/sketch_fake.hpp"
#endif
