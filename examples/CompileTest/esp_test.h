
#pragma once

#include "sdkconfig.h"

#if defined(ESP8266)
#include "esp_8266_test.h"
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
#include "esp_s3_test.h"
#endif