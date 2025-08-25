#ifdef ESP32

#include "platforms/esp/esp_version.h"
#include "fl/sketch_macros.h"


#if ESP_IDF_VERSION_5_OR_HIGHER
#include "platforms/esp/32/audio/i2s_audio_idf5.hpp"
#elif ESP_IDF_VERSION_4_OR_HIGHER
#include "platforms/esp/32/audio/i2s_audio_idf4.hpp"
#else
#include "platforms/esp/32/audio/i2s_audio_null.hpp"
#endif


#endif  // 
