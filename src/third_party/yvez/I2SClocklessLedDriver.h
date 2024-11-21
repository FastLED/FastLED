

#ifndef ESP32
#error "This library is for ESP32 only"
#endif

#include "esp32-hal.h"


// All of our FastLED specific hacks will go in here.
#ifndef USE_FASTLED
#define USE_FASTLED 1
#endif

// Hack to get lib to work in non debug mode.
#ifdef ESP_LOGE
#undef ESP_LOGE
#define ESP_LOGE(tag, ...)
#endif

#ifndef FASTLED_YVEZ_INTERNAL
#error "This file should only be included from platforms/esp/32/yvez_i2s.cpp"
#endif

// silence the warning above
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wattributes"  // silence warnings about the auto-generated iram1 section common on esp32.

#if CONFIG_IDF_TARGET_ESP32S3 || CONFIG_IDF_TARGET_ESP32
#include "third_party/yvez/I2SClocklessVirtualLedDriver/I2SClocklessVirtualLedDriver.h"
#else
#error "This library is for ESP32 or ESP32-S3 only"
#endif


// pop the warning silence
#pragma GCC diagnostic pop