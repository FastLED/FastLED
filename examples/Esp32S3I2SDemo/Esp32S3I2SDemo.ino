

#ifndef ESP32
#define IS_ESP32_S3 0
#else
#include "sdkconfig.h"

#ifdef CONFIG_IDF_TARGET_ESP32S3
#define IS_ESP32_S3 1
#else
#define IS_ESP32_S3 0
#endif  // CONFIG_IDF_TARGET_ESP32
#endif  // ESP32

#if IS_ESP32_S3
#include "Esp32S3I2SDemo.h"
#else
#include "platforms/sketch_fake.hpp"
#endif
