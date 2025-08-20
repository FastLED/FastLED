
#include "fl/has_define.h"

#if !__has_include(<Adafruit_NeoPixel.h>)
#include "platforms/adafruit/clockless_fake.hpp"
#else
#include "platforms/adafruit/clockless_real.hpp"
#endif
