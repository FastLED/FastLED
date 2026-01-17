// ok no namespace fl
#pragma once

// The WS2812 family of chipsets is special! Why?
// Because it's super cheap. So we optimize it heavily.
//
// After this header is included, the following will be defined:
// FASTLED_WS2812_HAS_SPECIAL_DRIVER (either 1 or 0)
// If FASTLED_WS2812_HAS_SPECIAL_DRIVER is 0, then a default driver
// will be used, otherwise the platform provides a special driver.

#include "fl/int.h"
#include "eorder.h"

#ifndef FASTLED_OVERCLOCK
#error "This needs to be included by chipsets.h when FASTLED_OVERCLOCK is defined"
#endif


#if defined(FASTLED_USES_ESP32S3_I2S)
#include "platforms/esp/32/drivers/i2s/clockless_i2s_esp32s3.h"
// Explicit name for I2S-based WS2812 controller (ESP32-S3)
template <fl::u8 DATA_PIN, EOrder RGB_ORDER = fl::GRB>
class WS2812I2S:
	public fl::ClocklessController_I2S_Esp32_WS2812<
		DATA_PIN,
		RGB_ORDER
	> {};
// Default WS2812 controller typedef (selects I2S on ESP32-S3)
template <fl::u8 DATA_PIN, EOrder RGB_ORDER = fl::GRB>
using WS2812Controller800Khz = WS2812I2S<DATA_PIN, RGB_ORDER>;
#define FASTLED_WS2812_HAS_SPECIAL_DRIVER 1
#elif defined(FASTLED_ESP32_LCD_DRIVER) && defined(CONFIG_IDF_TARGET_ESP32S3)
#include "platforms/esp/32/drivers/lcd_cam/clockless_lcd_i80_esp32.h"
// Explicit name for LCD I80-based WS2812 controller (ESP32-S3)
template <fl::u8 DATA_PIN, EOrder RGB_ORDER = fl::GRB>
class WS2812LCD_I80:
	public fl::ClocklessController_LCD_I80_WS2812<
		DATA_PIN,
		RGB_ORDER
	> {};
// Default WS2812 controller typedef (selects LCD I80 on ESP32-S3)
template <fl::u8 DATA_PIN, EOrder RGB_ORDER = fl::GRB>
using WS2812Controller800Khz = WS2812LCD_I80<DATA_PIN, RGB_ORDER>;
#define FASTLED_WS2812_HAS_SPECIAL_DRIVER 1
// NOTE: LCD RGB driver for ESP32-P4 is disabled (files are .disabled)
// When re-enabled, uncomment the following block:
// #elif defined(FASTLED_ESP32_LCD_RGB_DRIVER) && defined(CONFIG_IDF_TARGET_ESP32P4)
// #include "platforms/esp/32/drivers/lcd_cam/clockless_lcd_rgb_esp32.h"
// template <fl::u8 DATA_PIN, EOrder RGB_ORDER = fl::GRB>
// class WS2812LCD_RGB:
// 	public fl::ClocklessController_LCD_RGB_WS2812<DATA_PIN, RGB_ORDER> {};
// template <fl::u8 DATA_PIN, EOrder RGB_ORDER = fl::GRB>
// using WS2812Controller800Khz = WS2812LCD_RGB<DATA_PIN, RGB_ORDER>;
// #define FASTLED_WS2812_HAS_SPECIAL_DRIVER 1
#elif (defined(PICO_RP2040) || defined(PICO_RP2350) || defined(ARDUINO_ARCH_RP2040) || defined(ARDUINO_ARCH_RP2350)) && defined(FASTLED_RP2040_CLOCKLESS_PIO_AUTO)
#include "platforms/arm/rp/rpcommon/clockless_rp_pio_auto.h"
// Explicit name for RP2040/RP2350 PIO automatic parallel WS2812 controller
template <fl::u8 DATA_PIN, EOrder RGB_ORDER = fl::GRB>
class WS2812RP2040Auto:
	public fl::ClocklessController_RP2040_PIO_WS2812<
		DATA_PIN,
		RGB_ORDER
	> {};
// Default WS2812 controller typedef (selects automatic parallel PIO on RP2040/RP2350)
template <fl::u8 DATA_PIN, EOrder RGB_ORDER = fl::GRB>
using WS2812Controller800Khz = WS2812RP2040Auto<DATA_PIN, RGB_ORDER>;
#define FASTLED_WS2812_HAS_SPECIAL_DRIVER 1
#elif defined(FASTLED_USE_ADAFRUIT_NEOPIXEL)
#include "platforms/adafruit/clockless.h"
// Explicit name for Adafruit NeoPixel-based WS2812 controller
template <fl::u8 DATA_PIN, EOrder RGB_ORDER = fl::GRB>
class WS2812Adafruit : public fl::AdafruitWS2812Controller<DATA_PIN, RGB_ORDER> {};
// Default WS2812 controller typedef (selects Adafruit driver)
template <fl::u8 DATA_PIN, EOrder RGB_ORDER = fl::GRB>
using WS2812Controller800Khz = WS2812Adafruit<DATA_PIN, RGB_ORDER>;
#define FASTLED_WS2812_HAS_SPECIAL_DRIVER 1
#else
#define FASTLED_WS2812_HAS_SPECIAL_DRIVER 0
#endif  // defined(FASTLED_USES_ESP32S3_I2S)
