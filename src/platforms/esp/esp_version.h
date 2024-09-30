#if __has_include(<esp_idf_version.h>)
#include "esp_idf_version.h"
#else
// Older esp toolchains don't have this header, so we define it here.
#define ESP_IDF_VERSION_MAJOR 3
#define ESP_IDF_VERSION_MINOR 0
#define ESP_IDF_VERSION_PATCH 0

#define ESP_IDF_VERSION_VAL(major, minor, patch) ((major << 16) | (minor << 8) | (patch))
#define ESP_IDF_VERSION  ESP_IDF_VERSION_VAL(ESP_IDF_VERSION_MAJOR, \
                                             ESP_IDF_VERSION_MINOR, \
                                             ESP_IDF_VERSION_PATCH)
#endif
