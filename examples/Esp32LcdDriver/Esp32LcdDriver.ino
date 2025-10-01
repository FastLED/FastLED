/// @file Esp32LcdDriver.ino
/// @brief ESP32 LCD parallel driver example (LCD_CAM peripheral)

#ifndef ESP32
#define IS_ESP32_LCD 0
#else
#include "sdkconfig.h"

#if defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3)
#define IS_ESP32_LCD 1
#else
#define IS_ESP32_LCD 0
#endif
#endif  // ESP32

#if IS_ESP32_LCD
#include "Esp32LcdDriver.h"
#else
#include "platforms/sketch_fake.hpp"
#endif
