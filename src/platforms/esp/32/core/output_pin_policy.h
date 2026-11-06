#pragma once

// IWYU pragma: private
// SDK/user-mask output legality, independent of GPIO provider selection.
#include "platforms/esp/is_esp.h"
#include "platforms/esp/esp_version.h"
#include "fl/stl/stdint.h"
#include "fl/stl/static_assert.h"
#include "fl/stl/has_include.h"
#if FL_HAS_INCLUDE("soc/soc_caps.h")
#include "soc/soc_caps.h" // IWYU pragma: keep
#endif

#define _FL_BIT(B) (1ULL << B)

#ifndef FASTLED_UNUSABLE_PIN_MASK

// CONFIG_IDF_TARGET_ESP32 was introduced in ESP-IDF v4.0
// For v3.x (where the macro doesn't exist), assume original ESP32
// For v4.0+, only use these settings when CONFIG_IDF_TARGET_ESP32 is defined
#if !ESP_IDF_VERSION_4_OR_HIGHER || defined(FL_IS_ESP_32DEV)
// 40 GPIO pins. ESPIDF defined 24, 28-31 as invalid and 34-39 as readonly
// GPIO 6-11 used by default for SPI flash.  GPIO 20 is invalid.
// NOTE: GPIO 1 & 3 commonly used for UART and may cause flashes when uploading.
#define FASTLED_UNUSABLE_PIN_MASK (0ULL | _FL_BIT(6) | _FL_BIT(7) | _FL_BIT(8) | _FL_BIT(9) | _FL_BIT(10) | _FL_BIT(11) | _FL_BIT(20))

#elif defined(FL_IS_ESP_32C3)
// 22 GPIO pins. ESPIDF defines all pins as valid
// GPIO 11-17 used by default for SPI flash
// NOTE: GPIO 20-21 commonly used for UART and may cause flashes when uploading.
#define FASTLED_UNUSABLE_PIN_MASK (0ULL | _FL_BIT(11) | _FL_BIT(12) | _FL_BIT(13) | _FL_BIT(14) | _FL_BIT(15) | _FL_BIT(16) | _FL_BIT(17))

#elif defined(FL_IS_ESP_32S2)
// 48 GPIO pins.  ESPIDF defines 22-25, 47 as invalid and 46-47 as readonly.s
// GPIO 27-32 used by default for SPI flash.
// NOTE: GPIO 37 & 38 commonly used for UART and may cause flashes when uploading.
#define FASTLED_UNUSABLE_PIN_MASK (0ULL | _FL_BIT(27) | _FL_BIT(28) | _FL_BIT(29) | _FL_BIT(30) | _FL_BIT(31) | _FL_BIT(32))

#elif defined(FL_IS_ESP_32S3)
// 49 GPIO pins. ESPIDF defineds 22-25 as invalid.
// GPIO 19-20 used for USB-JTAG and will break flashing if used.
// GPIO 27-32 used by default for SPI flash.
// NOTE: GPIO 43 & 44 commonly used for UART and may cause flashes when uploading.
#define FASTLED_UNUSABLE_PIN_MASK (0ULL | _FL_BIT(19) | _FL_BIT(20) | _FL_BIT(27) | _FL_BIT(28) | _FL_BIT(29) | _FL_BIT(30) | _FL_BIT(31) | _FL_BIT(32))

#elif defined(FL_IS_ESP_32C5)

// GPIO 13,14 (USB_D*), 15-18,20-22 used by default for SPI flash.
#define FASTLED_UNUSABLE_PIN_MASK (0ULL | _FL_BIT(13) | _FL_BIT(14) | _FL_BIT(15) | _FL_BIT(16) | _FL_BIT(17) | _FL_BIT(18) | _FL_BIT(20) | _FL_BIT(21) | _FL_BIT(22))

#elif defined(FL_IS_ESP_32C6)

// GPIO 12/13 are native USB D-/D+ on ESP32-C6 USB-Serial/JTAG boards; using
// them as FastLED outputs can sever the active serial/upload connection.
// GPIO 24-26 and 28-30 are reserved/unusable on ESP32-C6 packages.
#define FASTLED_UNUSABLE_PIN_MASK (0ULL | _FL_BIT(12) | _FL_BIT(13) | _FL_BIT(24) | _FL_BIT(25) | _FL_BIT(26) | _FL_BIT(28) | _FL_BIT(29) | _FL_BIT(30))

#elif defined(FL_IS_ESP_32P4)
// 55 GPIO pins. ESPIDF defines all pins as valid.
// NOTE: GPIO 24 & 25 commonly used for USB and may cause flashes when uploading.
#define FASTLED_UNUSABLE_PIN_MASK (0ULL | _FL_BIT(24) | _FL_BIT(25))

#elif defined(FL_IS_ESP_32H2)
// 22 GPIO pins.  ESPIDF defines all pins as valid.
// GPIO 15-21 are typically reserved for SPI0/1 flash communication.
#define FASTLED_UNUSABLE_PIN_MASK (0ULL | _FL_BIT(15) | _FL_BIT(16) | _FL_BIT(17) | _FL_BIT(18) | _FL_BIT(19) | _FL_BIT(20) | _FL_BIT(21))
#elif defined(FL_IS_ESP_32C2)
// 21 GPIO pins (GPIO 0-20). ESPIDF defines all pins as valid.
// GPIO 11-17 are typically reserved for SPI flash and not recommended for other uses.
// NOTE: ESP32-C2 is a low-memory device - complex examples may not compile
#define FASTLED_UNUSABLE_PIN_MASK (0ULL | _FL_BIT(11) | _FL_BIT(12) | _FL_BIT(13) | _FL_BIT(14) | _FL_BIT(15) | _FL_BIT(16) | _FL_BIT(17))
#else
#warning Unknown ESP32 chip variant.  Only pins defined by ESP-IDF will be masked.
#define FASTLED_UNUSABLE_PIN_MASK (0ULL)
#endif



#endif

namespace fl {

// SOC GPIO mask was not added until version IDF version 4.3.  Prior to this only ESP32 chip was supported, so only
// the value for ESP32
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(4, 3, 0) && !defined(SOC_GPIO_VALID_OUTPUT_GPIO_MASK)
// 0~39 except from 24, 28~31 are valid
#define SOC_GPIO_VALID_GPIO_MASK (0xFFFFFFFFFFULL & ~(0ULL | _FL_BIT(24) | _FL_BIT(28) | _FL_BIT(29) | _FL_BIT(30) | _FL_BIT(31)))
// GPIO >= 34 are input only
#define SOC_GPIO_VALID_OUTPUT_GPIO_MASK (SOC_GPIO_VALID_GPIO_MASK & ~(0ULL | _FL_BIT(34) | _FL_BIT(35) | _FL_BIT(36) | _FL_BIT(37) | _FL_BIT(38) | _FL_BIT(39)))

#endif


// Define mask of valid pins.  Start with the list of valid output pins from ESPIDF and remove unusable pins
#define _FL_VALID_PIN_MASK (u64(SOC_GPIO_VALID_OUTPUT_GPIO_MASK) & ~FASTLED_UNUSABLE_PIN_MASK)

#define _FL_PIN_VALID(PIN) ((_FL_VALID_PIN_MASK & (1ULL << PIN)) != 0)

#if defined(FL_IS_ESP_32C6)
FL_STATIC_ASSERT(!_FL_PIN_VALID(12), "ESP32-C6 GPIO12 is USB D- and must be invalid for FastLED output");
FL_STATIC_ASSERT(!_FL_PIN_VALID(13), "ESP32-C6 GPIO13 is USB D+ and must be invalid for FastLED output");
#endif

// Peripheral pin legality does not depend on the selected software GPIO
// provider. Use the existing SDK output mask and user exclusions in both
// hardware consumers and native FastPin declarations. Range-specialize before
// forming a shift so negative and >=64 pin numbers are simply invalid.
template <int Pin, bool InRange = (Pin >= 0 && Pin < 64)>
struct Esp32OutputPinValidity {
    static constexpr bool value = false;
};
template <int Pin> struct Esp32OutputPinValidity<Pin, true> {
    static constexpr bool value = _FL_PIN_VALID(Pin);
};

} // namespace fl
