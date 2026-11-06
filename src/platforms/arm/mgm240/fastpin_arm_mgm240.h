/// @file fastpin_arm_mgm240.h
/// @brief FastPin implementation for Silicon Labs MGM240 ARM Cortex-M33
///
/// This implementation provides hardware-accelerated GPIO operations for the
/// MGM240SD22VNA microcontroller using Silicon Labs EMLIB.
///
/// Key features:
/// - Atomic GPIO operations using the P_SET/P_CLR register aliases
/// - Race condition-free pin manipulation in interrupt environments
/// - Direct Silicon Labs EMLIB integration for optimal performance
/// - Template-based compile-time optimization

#pragma once

// IWYU pragma: private

#include "fl/stl/stdint.h"

// Include Silicon Labs EMLIB GPIO for direct register access
// IWYU pragma: begin_keep
#include <em_gpio.h>
#include <em_cmu.h>
// IWYU pragma: end_keep

#include "led_sysdefs.h"
#include "fl/stl/compiler_control.h"
#include "fl/system/fastpin_base.h"
#include "fl/stl/noexcept.h"
#include "platforms/arm/mgm240/mgm240_pin.h"

namespace fl {
/// @brief Pin definition macro for MGM240 FastPin specializations
/// Creates a template specialization of FastPin for a specific Arduino pin number
/// @param PIN Arduino pin number (0-25)
/// @param BIT Pin number within the port (0-7)
/// @param PORT_LETTER Port letter (A, B, C, D)
/// @param MASK Bit mask for the pin (1 << BIT)
#define _FL_DEFPIN(PIN, BIT, PORT_LETTER, MASK) template<> class FastPin<PIN> : public _ARMPIN<MASK, __generated_struct_GPIO_PORT_##PORT_LETTER, PORT_NUM_##PORT_LETTER, BIT> {};

// Define port numbers for template parameters
#define PORT_NUM_A 0
#define PORT_NUM_B 1
#define PORT_NUM_C 2
#define PORT_NUM_D 3

// Pin mappings for Arduino Nano Matter (MGM240SD22VNA)
// Based on Arduino Nano form factor - verify against hardware documentation

// Digital pins 0-13 (Arduino Nano standard layout)
_FL_DEFPIN(0,  0, A, (1 << 0));   // D0/RX  - PA00
_FL_DEFPIN(1,  1, A, (1 << 1));   // D1/TX  - PA01
_FL_DEFPIN(2,  2, A, (1 << 2));   // D2     - PA02
_FL_DEFPIN(3,  3, A, (1 << 3));   // D3/PWM - PA03
_FL_DEFPIN(4,  4, A, (1 << 4));   // D4     - PA04
_FL_DEFPIN(5,  5, A, (1 << 5));   // D5/PWM - PA05
_FL_DEFPIN(6,  6, A, (1 << 6));   // D6/PWM - PA06
_FL_DEFPIN(7,  7, A, (1 << 7));   // D7     - PA07
_FL_DEFPIN(8,  0, B, (1 << 0));   // D8     - PB00
_FL_DEFPIN(9,  1, B, (1 << 1));   // D9/PWM - PB01
_FL_DEFPIN(10, 2, B, (1 << 2));   // D10/SS - PB02
_FL_DEFPIN(11, 3, B, (1 << 3));   // D11/MOSI - PB03
_FL_DEFPIN(12, 4, B, (1 << 4));   // D12/MISO - PB04
_FL_DEFPIN(13, 5, B, (1 << 5));   // D13/SCK/LED - PB05

// Analog pins A0-A7 (mapped to port C)
_FL_DEFPIN(14, 0, C, (1 << 0));   // A0 - PC00
_FL_DEFPIN(15, 1, C, (1 << 1));   // A1 - PC01
_FL_DEFPIN(16, 2, C, (1 << 2));   // A2 - PC02
_FL_DEFPIN(17, 3, C, (1 << 3));   // A3 - PC03
_FL_DEFPIN(18, 4, C, (1 << 4));   // A4/SDA - PC04
_FL_DEFPIN(19, 5, C, (1 << 5));   // A5/SCL - PC05
_FL_DEFPIN(20, 6, C, (1 << 6));   // A6 - PC06
_FL_DEFPIN(21, 7, C, (1 << 7));   // A7 - PC07

// Additional GPIO pins for extended functionality (port D)
_FL_DEFPIN(22, 0, D, (1 << 0));   // D22 - PD00
_FL_DEFPIN(23, 1, D, (1 << 1));   // D23 - PD01
_FL_DEFPIN(24, 2, D, (1 << 2));   // D24 - PD02
_FL_DEFPIN(25, 3, D, (1 << 3));   // D25 - PD03

#define HAS_HARDWARE_PIN_SUPPORT
}  // namespace fl
