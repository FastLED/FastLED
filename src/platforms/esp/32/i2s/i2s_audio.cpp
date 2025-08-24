#ifdef ESP32

#include "platforms/esp/esp_version.h"
#include "fl/sketch_macros.h"


#if ESP_IDF_VERSION_5_OR_HIGHER
#include "platforms/esp/32/i2s/i2s_audio_idf5.hpp"
#elif ESP_IDF_VERSION_4_OR_HIGHER
#include "platforms/esp/32/i2s/i2s_audio_idf4.hpp"
#else
#include "platforms/esp/32/i2s/i2s_audio_null.hpp"
#endif


// #define ESP_IDF_VERSION  ESP_IDF_VERSION_VAL(ESP_IDF_VERSION_MAJOR, \
//     ESP_IDF_VERSION_MINOR, \
//     ESP_IDF_VERSION_PATCH)

// // Try to display the actual values
// #pragma message("ESP_IDF_VERSION_MAJOR: " FASTLED_STRINGIFY(ESP_IDF_VERSION_MAJOR))
// #pragma message("ESP_IDF_VERSION_MINOR: " FASTLED_STRINGIFY(ESP_IDF_VERSION_MINOR))
// #pragma message("ESP_IDF_VERSION_PATCH: " FASTLED_STRINGIFY(ESP_IDF_VERSION_PATCH))

// // Also try runtime debugging
// // static const char* esp_version_info = "ESP_IDF_VERSION: " FASTLED_STRINGIFY(ESP_IDF_VERSION_MAJOR) "." FASTLED_STRINGIFY(ESP_IDF_VERSION_MINOR) "." FASTLED_STRINGIFY(ESP_IDF_VERSION_PATCH);

// // Try using the packed version
// #pragma message("ESP_IDF_VERSION (packed): " FASTLED_STRINGIFY(ESP_IDF_VERSION))

// // Check if macros are defined
// #ifdef ESP_IDF_VERSION_MAJOR
// #pragma message("ESP_IDF_VERSION_MAJOR is defined")
// #else
// #pragma message("ESP_IDF_VERSION_MAJOR is NOT defined")
// #endif

// #ifdef ESP_IDF_VERSION
// #pragma message("ESP_IDF_VERSION is defined")
// #else
// #pragma message("ESP_IDF_VERSION is NOT defined")
// #endif

// // Test FASTLED_STRINGIFY with a known value
// #pragma message("FASTLED_STRINGIFY test: " FASTLED_STRINGIFY(42))



#endif  // 
