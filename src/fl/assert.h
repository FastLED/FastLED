#pragma once



#include "fl/warn.h"
#include "fl/strstream.h"

#ifndef DEBUG
#define FASTLED_ASSERT(x, MSG) FASTLED_WARN_IF(!(x), MSG)
#else


#ifndef FASTLED_USES_SYSTEM_ASSERT
#if defined(FASTLED_TESTING)
#define FASTLED_USES_SYSTEM_ASSERT 1
#else
#define FASTLED_USES_SYSTEM_ASSERT 0
#endif
#endif

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
#elif FASTLED_USES_SYSTEM_ASSERT
#include <assert.h>
#define FASTLED_ASSERT(x, MSG) {\
        FASTLED_WARN_IF(!(x), MSG); \
        assert(x); \
}
#else
#define FASTLED_ASSERT(x, MSG) FASTLED_WARN_IF(!(x), MSG)
#endif
#endif
