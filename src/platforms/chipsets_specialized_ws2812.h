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


#if defined(__IMXRT1062__) && !defined(FASTLED_NOT_USES_OBJECTFLED)
#if defined(FASTLED_USES_OBJECTFLED)
#warning "FASTLED_USES_OBJECTFLED is now implicit for Teensy 4.0/4.1 for WS2812 and is no longer needed."
#endif
#include "platforms/arm/k20/clockless_objectfled.h"
template <fl::u8 DATA_PIN, EOrder RGB_ORDER = fl::GRB>
class WS2812Controller800Khz:
	public fl::ClocklessController_ObjectFLED_WS2812<
		DATA_PIN,
		RGB_ORDER> {
 public:
    typedef fl::ClocklessController_ObjectFLED_WS2812<DATA_PIN, RGB_ORDER> Base;
	WS2812Controller800Khz(): Base(FASTLED_OVERCLOCK) {}
};
#define FASTLED_WS2812_HAS_SPECIAL_DRIVER 1
#elif defined(FASTLED_USES_ESP32S3_I2S)
#include "platforms/esp/32/clockless_i2s_esp32s3.h"
template <fl::u8 DATA_PIN, EOrder RGB_ORDER = fl::GRB>
class WS2812Controller800Khz:
	public fl::ClocklessController_I2S_Esp32_WS2812<
		DATA_PIN,
		RGB_ORDER
	> {};
#define FASTLED_WS2812_HAS_SPECIAL_DRIVER 1
#elif defined(FASTLED_USE_ADAFRUIT_NEOPIXEL)
#include "platforms/adafruit/clockless.h"
template <fl::u8 DATA_PIN, EOrder RGB_ORDER = fl::GRB>
class WS2812Controller800Khz : public fl::AdafruitWS2812Controller<DATA_PIN, RGB_ORDER> {};
#define FASTLED_WS2812_HAS_SPECIAL_DRIVER 1
#else
#define FASTLED_WS2812_HAS_SPECIAL_DRIVER 0
#endif  // defined(FASTLED_USES_OBJECTFLED)
