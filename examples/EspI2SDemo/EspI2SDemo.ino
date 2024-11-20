#ifdef ESP32  // This example is only for the ESP32

#include "esp32-hal.h"

#if !(CONFIG_IDF_TARGET_ESP32S3 || CONFIG_IDF_TARGET_ESP32)
// this is only for ESP32 and ESP32-S3
#else

#include <FastLED.h>
#include "platforms/esp/32/yvez_i2s.h"

void setup() {
    // TODO: Implement
}

void loop() {
    // TODO: implement
    FastLED.delay(1000);
}

#endif  // CONFIG_IDF_TARGET_ESP32S3 || CONFIG_IDF_TARGET_ESP32
#endif  // ESP32