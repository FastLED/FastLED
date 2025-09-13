
#include "fl/has_include.h"

#if FL_HAS_INCLUDE(<Adafruit_NeoPixel.h>)
#include "platforms/adafruit/clockless_real.hpp"
#else
#include "platforms/adafruit/clockless_fake.hpp"
#endif
