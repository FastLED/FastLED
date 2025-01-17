#if __has_include(<esp_idf_version.h>)
#include "esp_idf_version.h"
#else
// Older esp toolchains don't have this header, so we define it here.


#ifndef ESP_IDF_VERSION_MAJOR
#define ESP_IDF_VERSION_MAJOR 3
#endif
#ifndef ESP_IDF_VERSION_MINOR
#define ESP_IDF_VERSION_MINOR 0
#endif
#ifndef ESP_IDF_VERSION_PATCH
#define ESP_IDF_VERSION_PATCH 0
#endif

#ifndef ESP_IDF_VERSION_VAL
#define ESP_IDF_VERSION_VAL(major, minor, patch) ((major << 16) | (minor << 8) | (patch))
#endif

#ifndef ESP_IDF_VERSION
#define ESP_IDF_VERSION  ESP_IDF_VERSION_VAL(ESP_IDF_VERSION_MAJOR, \
                                             ESP_IDF_VERSION_MINOR, \
                                             ESP_IDF_VERSION_PATCH)
#endif
#endif


