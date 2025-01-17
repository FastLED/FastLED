#pragma once



#include "fl/warn.h"
#include "fl/strstream.h"

#ifndef DEBUG
#define FASTLED_ASSERT(x, MSG) FASTLED_WARN_IF(!(x), MSG)
#else

#ifdef ESP32
#include "esp_log.h"
#include "esp_check.h"
#define FASTLED_ASSERT(x, MSG)                                                \
    {                                                                         \
        if (!(x)) {                                                           \
            ESP_LOGE("#### FastLED", "%s", (fl::StrStream() << MSG).c_str()); \
            ESP_ERROR_CHECK(ESP_FAIL);                                        \
        }                                                                     \
    }
#else
#define FASTLED_ASSERT(x, MSG) FASTLED_WARN_IF(!(x), MSG)
#endif
#endif
