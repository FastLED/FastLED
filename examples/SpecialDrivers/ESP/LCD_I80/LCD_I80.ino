/// @file LCD_I80.ino
/// @brief ESP32 LCD I80 parallel driver example

#ifndef ESP32
#define IS_ESP32_LCD_I80 0
#else
#include "sdkconfig.h"

#if defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3)
#define IS_ESP32_LCD_I80 1
#else
#define IS_ESP32_LCD_I80 0
#endif
#endif  // ESP32

#if IS_ESP32_LCD_I80
#include "LCD_I80.h"
#else
#include "platforms/sketch_fake.hpp"
#endif
