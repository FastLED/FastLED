#pragma once

#include "sdkconfig.h"
#include "esp32_general_test.h"

#if defined(ESP8266)
#include "esp_8266_test.h"
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
#include "esp_s3_test.h"
#elif defined(CONFIG_IDF_TARGET_ESP32S2)
// ESP32-S2 uses same tests as general ESP32 for now
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
// ESP32-C3 uses same tests as general ESP32 for now
#elif defined(CONFIG_IDF_TARGET_ESP32C6)
// ESP32-C6 uses same tests as general ESP32 for now
#elif defined(CONFIG_IDF_TARGET_ESP32H2)
// ESP32-H2 uses same tests as general ESP32 for now
#elif defined(ESP32)
// Generic ESP32
#endif

void esp_tests() {
#if defined(ESP8266)
    esp_8266_tests();
#elif defined(ESP32)
    esp32_general_tests();
    #if defined(CONFIG_IDF_TARGET_ESP32S3)
    esp32_s3_tests();
    #endif
#endif
}
