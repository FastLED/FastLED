#pragma once

#include <stdint.h>

#include "fl/register.h"
#include "fl/namespace.h"
#include "platforms/esp/esp_version.h"
#include "fastpin.h"

FASTLED_NAMESPACE_BEGIN

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wvolatile"

template<uint8_t PIN, uint32_t MASK, bool VALIDPIN> class _ESPPIN {
public:
  typedef volatile uint32_t * port_ptr_t;
  typedef uint32_t port_t;

  static constexpr bool validpin() { return VALIDPIN; }

#ifndef GPIO_OUT1_REG
  static constexpr uint32_t GPIO_REG = GPIO_OUT_REG;
  static constexpr uint32_t GPIO_BIT_SET_REG = GPIO_OUT_W1TS_REG;
  static constexpr uint32_t GPIO_BIT_CLEAR_REG = GPIO_OUT_W1TC_REG;
  #else
  static constexpr uint32_t GPIO_REG = PIN < 32 ? GPIO_OUT_REG : GPIO_OUT1_REG;
  static constexpr uint32_t GPIO_BIT_SET_REG = PIN < 32 ? GPIO_OUT_W1TS_REG : GPIO_OUT1_W1TS_REG;
  static constexpr uint32_t GPIO_BIT_CLEAR_REG = PIN < 32 ? GPIO_OUT_W1TC_REG : GPIO_OUT1_W1TC_REG;
  #endif

  inline static void setOutput() {
      static_assert(validpin(), "Invalid pin specified");
      pinMode(PIN, OUTPUT);
  }
  inline static void setInput() { pinMode(PIN, INPUT); }

  inline static void hi() __attribute__ ((always_inline)) {
      *sport() = MASK;
  }

  inline static void lo() __attribute__ ((always_inline)) {
      *cport() = MASK;
  }

  inline static void set(FASTLED_REGISTER port_t val) __attribute__ ((always_inline)) {
      *port() = val;
  }

  inline static void strobe() __attribute__ ((always_inline)) { toggle(); toggle(); }

  inline static void toggle() __attribute__ ((always_inline)) {
      *port() ^= MASK;
  }

  inline static void hi(FASTLED_REGISTER port_ptr_t port) __attribute__ ((always_inline)) { hi(); }
  inline static void lo(FASTLED_REGISTER port_ptr_t port) __attribute__ ((always_inline)) { lo(); }
  inline static void fastset(FASTLED_REGISTER port_ptr_t port, FASTLED_REGISTER port_t val) __attribute__ ((always_inline)) { *port = val; }

  inline static port_t hival() __attribute__ ((always_inline)) {
      return (*port()) | MASK;
  }

  inline static port_t loval() __attribute__ ((always_inline)) {
      return (*port()) & ~MASK;
  }

  inline static port_ptr_t port() __attribute__ ((always_inline)) {
      return (port_ptr_t)GPIO_REG;
  }

  inline static port_ptr_t sport() __attribute__ ((always_inline)) {
      return (port_ptr_t)GPIO_BIT_SET_REG;
  }

  inline static port_ptr_t cport() __attribute__ ((always_inline)) {
      return (port_ptr_t)GPIO_BIT_CLEAR_REG;
  }

  inline static port_t mask() __attribute__ ((always_inline)) { return MASK; }

  inline static bool isset() __attribute__ ((always_inline)) {
      return (*port()) & MASK;
  }
};

#ifndef FASTLED_UNUSABLE_PIN_MASK

#define _FL_BIT(B) (1ULL << B)

#if CONFIG_IDF_TARGET_ESP32
// 40 GPIO pins. ESPIDF defined 24, 28-31 as invalid and 34-39 as readonly
// GPIO 6-11 used by default for SPI flash.  GPIO 20 is invalid.
// NOTE: GPIO 1 & 3 commonly used for UART and may cause flashes when uploading.
#define FASTLED_UNUSABLE_PIN_MASK (0ULL | _FL_BIT(6) | _FL_BIT(7) | _FL_BIT(8) | _FL_BIT(9) | _FL_BIT(10) | _FL_BIT(20))

#elif CONFIG_IDF_TARGET_ESP32C3
// 22 GPIO pins. ESPIDF defines all pins as valid
// GPIO 11-17 used by default for SPI flash
// NOTE: GPIO 20-21 commonly used for UART and may cause flashes when uploading.
#define FASTLED_UNUSABLE_PIN_MASK (0ULL | _FL_BIT(11) | _FL_BIT(12) | _FL_BIT(13) | _FL_BIT(14) | _FL_BIT(15) | _FL_BIT(16) | _FL_BIT(17))

#elif CONFIG_IDF_TARGET_ESP32S2
// 48 GPIO pins.  ESPIDF defines 22-25, 47 as invalid and 46-47 as readonly.s
// GPIO 27-32 used by default for SPI flash.
// NOTE: GPIO 37 & 38 commonly used for UART and may cause flashes when uploading.
#define FASTLED_UNUSABLE_PIN_MASK (0ULL | _FL_BIT(27) | _FL_BIT(28) | _FL_BIT(29) | _FL_BIT(30) | _FL_BIT(31) | _FL_BIT(32))

#elif CONFIG_IDF_TARGET_ESP32S3
// 49 GPIO pins. ESPIDF defineds 22-25 as invalid.
// GPIO 27-32 used by default for SPI flash.
// NOTE: GPIO 43 & 44 commonly used for UART and may cause flashes when uploading.
#define FASTLED_UNUSABLE_PIN_MASK (0ULL | _FL_BIT(27) | _FL_BIT(28) | _FL_BIT(29) | _FL_BIT(30) | _FL_BIT(31) | _FL_BIT(32))

#elif CONFIG_IDF_TARGET_ESP32C6

// GPIO 20-22, 24-26 used by default for SPI flash.
#define FASTLED_UNUSABLE_PIN_MASK (0ULL |  _FL_BIT(24) | _FL_BIT(25) | _FL_BIT(26) | _FL_BIT(28) | _FL_BIT(29) | _FL_BIT(30))

#elif CONFIG_IDF_TARGET_ESP32H2
// 22 GPIO pins.  ESPIDF defines all pins as valid.
// ESP32-H2 datasheet not yet available, when it is, mask the pins commonly used by SPI flash.
#warning ESP32-H2 chip flash configuration not yet known.  Only pins defined by ESP-IDF will be masked.
#define FASTLED_UNUSABLE_PIN_MASK (0ULL)
#elif CONFIG_IDF_TARGET_ESP32C2
#warning ESP32-C2 chip variant is in beta support.  Only pins defined by ESP-IDF will be masked.
#define FASTLED_UNUSABLE_PIN_MASK (0ULL)
#else
#warning Unknown ESP32 chip variant.  Only pins defined by ESP-IDF will be masked.
#define FASTLED_UNUSABLE_PIN_MASK (0ULL)
#endif

#endif



// SOC GPIO mask was not added until version IDF version 4.3.  Prior to this only ESP32 chip was supported, so only
// the value for ESP32
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(4, 3, 0) && !defined(SOC_GPIO_VALID_OUTPUT_GPIO_MASK)
// 0~39 except from 24, 28~31 are valid
#define SOC_GPIO_VALID_GPIO_MASK (0xFFFFFFFFFFULL & ~(0ULL | _FL_BIT(24) | _FL_BIT(28) | _FL_BIT(29) | _FL_BIT(30) | _FL_BIT(31)))
// GPIO >= 34 are input only
#define SOC_GPIO_VALID_OUTPUT_GPIO_MASK (SOC_GPIO_VALID_GPIO_MASK & ~(0ULL | _FL_BIT(34) | _FL_BIT(35) | _FL_BIT(36) | _FL_BIT(37) | _FL_BIT(38) | _FL_BIT(39)))

#endif


// Define mask of valid pins.  Start with the list of valid output pins from ESPIDF and remove unusable pins
#define _FL_VALID_PIN_MASK (uint64_t(SOC_GPIO_VALID_OUTPUT_GPIO_MASK) & ~FASTLED_UNUSABLE_PIN_MASK)

#define _FL_PIN_VALID(PIN) ((_FL_VALID_PIN_MASK & (1ULL << PIN)) != 0)

#define _FL_DEFPIN(PIN) template <> class FastPin<PIN> : public _ESPPIN<PIN, ((uint32_t)1 << (PIN % 32)), _FL_PIN_VALID(PIN)> {};

// Define all possible pins.  If the pin is not valid for a particular ESP32 variant, the pin number
// will be shifted into the 192-255 range, in effect rendering it unusable.
_FL_DEFPIN( 0); _FL_DEFPIN( 1); _FL_DEFPIN( 2); _FL_DEFPIN( 3);
_FL_DEFPIN( 4); _FL_DEFPIN( 5); _FL_DEFPIN( 6); _FL_DEFPIN( 7);
_FL_DEFPIN( 8); _FL_DEFPIN( 9); _FL_DEFPIN(10); _FL_DEFPIN(11);
_FL_DEFPIN(12); _FL_DEFPIN(13); _FL_DEFPIN(14); _FL_DEFPIN(15);
_FL_DEFPIN(16); _FL_DEFPIN(17); _FL_DEFPIN(18); _FL_DEFPIN(19);
_FL_DEFPIN(20); _FL_DEFPIN(21); _FL_DEFPIN(22); _FL_DEFPIN(23);
_FL_DEFPIN(24); _FL_DEFPIN(25); _FL_DEFPIN(26); _FL_DEFPIN(27);
_FL_DEFPIN(28); _FL_DEFPIN(29); _FL_DEFPIN(30); _FL_DEFPIN(31);
_FL_DEFPIN(32); _FL_DEFPIN(33); _FL_DEFPIN(34); _FL_DEFPIN(35);
_FL_DEFPIN(36); _FL_DEFPIN(37); _FL_DEFPIN(38); _FL_DEFPIN(39);
_FL_DEFPIN(40); _FL_DEFPIN(41); _FL_DEFPIN(42); _FL_DEFPIN(43);
_FL_DEFPIN(44); _FL_DEFPIN(45); _FL_DEFPIN(46); _FL_DEFPIN(47);
_FL_DEFPIN(48); _FL_DEFPIN(49); _FL_DEFPIN(50); _FL_DEFPIN(51);
_FL_DEFPIN(52); _FL_DEFPIN(53); _FL_DEFPIN(54); _FL_DEFPIN(55);
_FL_DEFPIN(56); _FL_DEFPIN(57); _FL_DEFPIN(58); _FL_DEFPIN(59);
_FL_DEFPIN(60); _FL_DEFPIN(61); _FL_DEFPIN(62); _FL_DEFPIN(63);

#define HAS_HARDWARE_PIN_SUPPORT

#pragma GCC diagnostic pop

FASTLED_NAMESPACE_END
