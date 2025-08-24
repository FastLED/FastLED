#ifdef ESP32

#include "platforms/esp/esp_version.h"

#define USE_IDF4 1
#define USE_IDF5 0

#if ESP_IDF_VERSION_5_OR_HIGHER
#include "platforms/esp/32/i2s/i2s_audio_idf4.hpp"
#elif ESP_IDF_VERSION_4_OR_HIGHER
#include "platforms/esp/32/i2s/i2s_audio_idf5.hpp"
#else
#include "platforms/esp/32/i2s/i2s_audio_null.hpp"
#endif
