#pragma once

/// @file esp32s3_lcd_integration.h
/// Integration header for ESP32-S3 LCD/I80 parallel LED driver
/// 
/// This file provides the integration point for the ESP32-S3 LCD parallel
/// driver with the FastLED library's controller system.

#include "sdkconfig.h"

#ifdef CONFIG_IDF_TARGET_ESP32S3

#include "esp32s3_clockless_i2s.h"

// Define controller aliases for easy use with FastLED.addLeds()
#define WS2812_LCD_ESP32S3 fl::ClocklessController_LCD_ESP32S3
#define WS2816_LCD_ESP32S3 fl::ClocklessController_LCD_ESP32S3
#define WS2813_LCD_ESP32S3 fl::ClocklessController_LCD_ESP32S3

// Enable the driver by defining this before including FastLED.h
#ifdef FASTLED_ESP32S3_LCD_DRIVER

FASTLED_NAMESPACE_BEGIN

/// @brief Convenience typedef for WS2812 on ESP32-S3 LCD driver
template<int DATA_PIN, EOrder RGB_ORDER = GRB>
using WS2812_LCD = ClocklessController_LCD_ESP32S3<DATA_PIN, RGB_ORDER, LedChipset::WS2812>;

/// @brief Convenience typedef for WS2816 on ESP32-S3 LCD driver  
template<int DATA_PIN, EOrder RGB_ORDER = GRB>
using WS2816_LCD = ClocklessController_LCD_ESP32S3<DATA_PIN, RGB_ORDER, LedChipset::WS2816>;

/// @brief Convenience typedef for WS2813 on ESP32-S3 LCD driver
template<int DATA_PIN, EOrder RGB_ORDER = GRB>
using WS2813_LCD = ClocklessController_LCD_ESP32S3<DATA_PIN, RGB_ORDER, LedChipset::WS2813>;

FASTLED_NAMESPACE_END

#endif // FASTLED_ESP32S3_LCD_DRIVER

#endif // CONFIG_IDF_TARGET_ESP32S3