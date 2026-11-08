// IWYU pragma: private

#ifndef __FASTPIN_ARM_RP2350_H
#define __FASTPIN_ARM_RP2350_H

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
#include "platforms/arm/rp/rp2350/rp2350_pin.h"

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

// The mask is the pin's bit within its bank: GPIO32-47 are bits 0-15 of the
// GPIO_HI registers, which _RP2350PIN selects for PIN >= 32 (#5100).
#define _FL_DEFPIN(PIN) template<> class FastPin<PIN> : public _RP2350PIN<PIN, (1u << ((PIN) & 31u))> {};

// RP2350B has up to 48 pins (0-47), RP2350A has 30 pins (0-29)
// We support up to 47 for all RP2350 variants
#define MAX_PIN 47

// Define all 48 pins (0-47) for RP2350 variants
_FL_DEFPIN(0); _FL_DEFPIN(1); _FL_DEFPIN(2); _FL_DEFPIN(3);
_FL_DEFPIN(4); _FL_DEFPIN(5); _FL_DEFPIN(6); _FL_DEFPIN(7);
_FL_DEFPIN(8); _FL_DEFPIN(9); _FL_DEFPIN(10); _FL_DEFPIN(11);
_FL_DEFPIN(12); _FL_DEFPIN(13); _FL_DEFPIN(14); _FL_DEFPIN(15);
_FL_DEFPIN(16); _FL_DEFPIN(17); _FL_DEFPIN(18); _FL_DEFPIN(19);
_FL_DEFPIN(20); _FL_DEFPIN(21); _FL_DEFPIN(22); _FL_DEFPIN(23);
_FL_DEFPIN(24); _FL_DEFPIN(25); _FL_DEFPIN(26); _FL_DEFPIN(27);
_FL_DEFPIN(28); _FL_DEFPIN(29); _FL_DEFPIN(30); _FL_DEFPIN(31);
_FL_DEFPIN(32); _FL_DEFPIN(33); _FL_DEFPIN(34); _FL_DEFPIN(35);
_FL_DEFPIN(36); _FL_DEFPIN(37); _FL_DEFPIN(38); _FL_DEFPIN(39);
_FL_DEFPIN(40); _FL_DEFPIN(41); _FL_DEFPIN(42); _FL_DEFPIN(43);
_FL_DEFPIN(44); _FL_DEFPIN(45); _FL_DEFPIN(46); _FL_DEFPIN(47);

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

#endif // __FASTPIN_ARM_RP2350_H
