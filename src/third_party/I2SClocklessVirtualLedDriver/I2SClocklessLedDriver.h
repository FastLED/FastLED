

#ifndef ESP32
#error "This library is for ESP32 only"
#endif

#include "esp32-hal.h"


// All of our FastLED specific hacks will go in here.
#ifndef USE_FASTLED
#define USE_FASTLED 1
#endif


#include "platforms/esp/esp_version.h"


#if CONFIG_IDF_TARGET_ESP32S3
#error "ESP32-S3 is not supported by this library"
#elif CONFIG_IDF_TARGET_ESP32C3
#error "ESP32-C3 is not supported by this library"
#elif CONFIG_IDF_TARGET_ESP32H2
#error "ESP32-H2 is not supported by this library"
#elif CONFIG_IDF_TARGET_ESP32C6
#error "ESP32-C6 is not supported by this library"
#endif

// esp32 dev
#if CONFIG_IDF_TARGET_ESP32
#include "espdev/I2SClocklessLedDriver.h"
#endif


