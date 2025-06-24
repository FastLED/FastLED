#pragma once


#if defined(ESP8266)
#include "esp_8266_test.h"
#else
#include "sdkconfig.h"
#if defined(CONFIG_IDF_TARGET_ESP32S3)
#include "esp_s3_test.h"
#elif defined(CONFIG_IDF_TARGET_ESP32S2)
#include "esp32_general_test.h"
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
#include "esp32_general_test.h"
#elif defined(CONFIG_IDF_TARGET_ESP32C6)
#include "esp32_general_test.h"
#elif defined(CONFIG_IDF_TARGET_ESP32H2)
#include "esp32_general_test.h"
#elif defined(ESP32)
#include "esp32_general_test.h"
#else
#error "Unexpected state"
#endif
#endif  // ESP8266

void esp_tests() {

}
