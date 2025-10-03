/// @file Esp32P4Parlio.ino
/// @brief ESP32-P4 PARLIO parallel driver example

#ifndef ESP32
#define IS_ESP32_P4 0
#else
#include "sdkconfig.h"

#if defined(CONFIG_IDF_TARGET_ESP32P4)
#define IS_ESP32_P4 1
#else
#define IS_ESP32_P4 0
#endif
#endif  // ESP32

#if IS_ESP32_P4
#include "Esp32P4Parlio.h"
#else
#include "platforms/sketch_fake.hpp"
#endif
