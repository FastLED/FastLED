/// @file LCD_RGB.ino
/// @brief ESP32-P4 LCD RGB parallel driver example

#ifndef ESP32
#define IS_ESP32P4_LCD_RGB 0
#else
#include "sdkconfig.h"

#if defined(CONFIG_IDF_TARGET_ESP32P4)
#define IS_ESP32P4_LCD_RGB 1
#else
#define IS_ESP32P4_LCD_RGB 0
#endif
#endif  // ESP32

#if IS_ESP32P4_LCD_RGB
#include "LCD_RGB.h"
#else
#include "platforms/sketch_fake.hpp"
#endif
