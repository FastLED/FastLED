#pragma once

#include "fl/has_include.h"

// Platform must be esp32
#if !defined(ESP32) || !FL_HAS_INCLUDE("sdkconfig.h")
#define FASTLED_HAS_AUDIO_INPUT 0
#else
// Include ESP version detection to check IDF version
#include "platforms/esp/esp_version.h"
// soc/soc_caps.h was introduced in ESP-IDF 4.0, not available in 3.3
#if ESP_IDF_VERSION_4_OR_HIGHER && FL_HAS_INCLUDE("soc/soc_caps.h")
#include "soc/soc_caps.h"
#if SOC_I2S_SUPPORTED
#define FASTLED_HAS_AUDIO_INPUT 1
#else
#define FASTLED_HAS_AUDIO_INPUT 0
#endif
#else
// ESP-IDF 3.3 and older - audio input not supported
#define FASTLED_HAS_AUDIO_INPUT 0
#endif
#endif  // ESP32
