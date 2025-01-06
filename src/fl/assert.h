#pragma once


#include "esp_log.h"
#include "esp_check.h"
#include "fl/warn.h"
#include "fl/strstream.h"

#ifdef DEBUG
#define FASTLED_ASSERT(x, MSG)                                                \
    {                                                                         \
        if (!(x)) {                                                           \
            ESP_LOGE("#### FastLED", "%s", (fl::StrStream() << MSG).c_str()); \
            ESP_ERROR_CHECK(ESP_FAIL);                                        \
        }                                                                     \
    }
#else
#define FASTLED_ASSERT(x, MSG)
#endif
