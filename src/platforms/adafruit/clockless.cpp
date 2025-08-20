
#include "fl/has_define.h"

#if !__has_include("Adafruit_NeoPixel.h")
#include "./clockless_fake.hpp"
#else
#include "./clockless_real.hpp"
#endif
