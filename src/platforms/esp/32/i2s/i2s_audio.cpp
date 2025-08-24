#ifdef ESP32

#include "platforms/esp/esp_version.h"


#if ESP_IDF_VERSION_5_OR_HIGHER
#include "platforms/esp/32/i2s/i2s_audio_idf4.hpp"
#elif ESP_IDF_VERSION_4_OR_HIGHER
#include "platforms/esp/32/i2s/i2s_audio_idf5.hpp"
#else
#include "platforms/esp/32/i2s/i2s_audio_null.hpp"
#endif


// #define ESP_IDF_VERSION  ESP_IDF_VERSION_VAL(ESP_IDF_VERSION_MAJOR, \
//     ESP_IDF_VERSION_MINOR, \
//     ESP_IDF_VERSION_PATCH)

#warning "ESP_IDF_VERSION_MAJOR: " ESP_IDF_VERSION_MAJOR
#warning "ESP_IDF_VERSION_MINOR: " ESP_IDF_VERSION_MINOR
#warning "ESP_IDF_VERSION_PATCH: " ESP_IDF_VERSION_PATCH


#endif  // 
