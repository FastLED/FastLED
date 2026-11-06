// IWYU pragma: private

#ifndef __FASTPIN_ARM_RP2040_H
#define __FASTPIN_ARM_RP2040_H

// IWYU pragma: begin_keep
#include <pico.h>
// IWYU pragma: end_keep
// IWYU pragma: begin_keep
#include "hardware/gpio.h"
// IWYU pragma: end_keep
// IWYU pragma: begin_keep
#include "hardware/structs/sio.h"
// IWYU pragma: end_keep
#include "fl/system/fastpin_base.h"
#include "fl/stl/compiler_control.h"
#include "platforms/arm/rp/rp2040/rp2040_pin.h"

FL_DISABLE_WARNING_PUSH
FL_DISABLE_WARNING_DEPRECATED_REGISTER
namespace fl {
// Forward declare FastPin template to support specializations below
template<u8 PIN> class FastPin;

#if defined(FASTLED_FORCE_SOFTWARE_PINS)
#warning "Software pin support forced, pin access will be slightly slower."
#define NO_HARDWARE_PIN_SUPPORT
#undef HAS_HARDWARE_PIN_SUPPORT

#else

// Use 64-bit literal for pin mask calculation to support RP2350B pins 32-47
// The template parameter is still uint32_t to maintain register compatibility
#define _FL_DEFPIN(PIN) template<> class FastPin<PIN> : public _RP2040PIN<PIN, (u32)(1ULL << PIN)> {};

// Set MAX_PIN based on platform capability
#if defined(PICO_RP2350)
// RP2350B has up to 48 pins (0-47), RP2350A has 30 pins (0-29)
// We support up to 47 for RP2350 variants
#define MAX_PIN 47
#else
// RP2040 has 30 pins (0-29)
#define MAX_PIN 29
#endif

// Define pins 0-29 for all RP2040/RP2350 variants
_FL_DEFPIN(0); _FL_DEFPIN(1); _FL_DEFPIN(2); _FL_DEFPIN(3);
_FL_DEFPIN(4); _FL_DEFPIN(5); _FL_DEFPIN(6); _FL_DEFPIN(7);
_FL_DEFPIN(8); _FL_DEFPIN(9); _FL_DEFPIN(10); _FL_DEFPIN(11);
_FL_DEFPIN(12); _FL_DEFPIN(13); _FL_DEFPIN(14); _FL_DEFPIN(15);
_FL_DEFPIN(16); _FL_DEFPIN(17); _FL_DEFPIN(18); _FL_DEFPIN(19);
_FL_DEFPIN(20); _FL_DEFPIN(21); _FL_DEFPIN(22); _FL_DEFPIN(23);
_FL_DEFPIN(24); _FL_DEFPIN(25); _FL_DEFPIN(26); _FL_DEFPIN(27);
_FL_DEFPIN(28); _FL_DEFPIN(29);

// Define additional pins 30-47 for RP2350 variants with extended GPIO
#if defined(PICO_RP2350)
_FL_DEFPIN(30); _FL_DEFPIN(31); _FL_DEFPIN(32); _FL_DEFPIN(33);
_FL_DEFPIN(34); _FL_DEFPIN(35); _FL_DEFPIN(36); _FL_DEFPIN(37);
_FL_DEFPIN(38); _FL_DEFPIN(39); _FL_DEFPIN(40); _FL_DEFPIN(41);
_FL_DEFPIN(42); _FL_DEFPIN(43); _FL_DEFPIN(44); _FL_DEFPIN(45);
_FL_DEFPIN(46); _FL_DEFPIN(47);
#endif

#ifdef PICO_DEFAULT_SPI_TX_PIN
#define SPI_DATA PICO_DEFAULT_SPI_TX_PIN
#else
#define SPI_DATA 19
#endif

#ifdef PICO_DEFAULT_SPI_SCK_PIN
#define SPI_CLOCK PICO_DEFAULT_SPI_SCK_PIN
#else
#define SPI_CLOCK 18
#endif

#define HAS_HARDWARE_PIN_SUPPORT

#endif // FASTLED_FORCE_SOFTWARE_PINS
}  // namespace fl

FL_DISABLE_WARNING_POP

#endif // __FASTPIN_ARM_RP2040_H
