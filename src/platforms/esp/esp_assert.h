#pragma once

#include "esp_check.h"
#include "esp_log.h"

#define FASTLED_ASSERT(x, MSG)                                                 \
    {                                                                          \
        if (!(x)) {                                                            \
            ESP_LOGE("#### FastLED", "%s", (fl::StrStream() << MSG).c_str());  \
            ESP_ERROR_CHECK(ESP_FAIL);                                         \
        }                                                                      \
    }