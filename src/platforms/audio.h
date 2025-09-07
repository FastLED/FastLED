#pragma once

#include "fl/has_define.h"

// Platform must be esp32
#if !defined(ESP32) || !__has_include("sdkconfig.h")
#define FASTLED_HAS_AUDIO_INPUT 0
#else
#include "soc/soc_caps.h"
#if SOC_I2S_SUPPORTED
#define FASTLED_HAS_AUDIO_INPUT 1
#else
#define FASTLED_HAS_AUDIO_INPUT 0
#endif
#endif  // ESP32
