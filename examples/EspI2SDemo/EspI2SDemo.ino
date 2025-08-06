
#include "fl/has_define.h"


// Platform must be esp32.
#if !defined(ESP32) || !__has_include("sdkconfig.h")
#define IS_ESP32_DEV 0
#else

#include "sdkconfig.h"

#if CONFIG_IDF_TARGET_ESP32
#define IS_ESP32_DEV 1
#else
#define IS_ESP32_DEV 0
#endif
#endif



#if IS_ESP32_DEV
#include "EspI2SDemo.h"
#else
#include "platforms/sketch_fake.hpp"
#endif
