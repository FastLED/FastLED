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
#include "fl/stl/static_assert.h"
#include "fl/system/fastpin_base.h"
#include "fl/stl/noexcept.h"
#include "platforms/arm/mgm240/mgm240_pin.h"

namespace fl {
/// Native pin for an MGM240 Arduino pin, specialized by _FL_DEFPIN for every
/// mapped pin. The timing-critical clockless and ezWS2812 drivers use it, so
/// they keep native GPIO when FASTLED_FORCE_SOFTWARE_PINS replaces FastPin.
/// The primary is instantiated only for unmapped pins.
template <u8 PIN> class Mgm240HwPin {
public:
    // Stubs keep the static assertion the only diagnostic.
    typedef volatile u32* port_ptr_t;
    typedef u32 port_t;
    static void setOutput() {}
    static void hi() {}
    static void lo() {}
    static port_t mask() { return 0; }
    static port_ptr_t port() { return nullptr; }
    static void fastset(port_ptr_t, port_t) {}
    FL_STATIC_ASSERT(PIN == 255 && PIN != 255,
                     "This pin is not in the MGM240 pin map (fastpin_arm_mgm240.h), so the clockless drivers cannot drive it.");
};

/// @brief Pin definition macro for MGM240 pin specializations
/// @param PIN Arduino pin number (0-25)
/// @param BIT Pin number within the port (0-7)
/// @param PORT_LETTER Port letter (A, B, C, D)
/// @param MASK Bit mask for the pin (1 << BIT)
// _FL_DEFPIN always emits Mgm240HwPin<PIN>. FastPin<PIN> derives from it only
// when FASTLED_FORCE_SOFTWARE_PINS is not set: an explicit specialization
// would override the forced software primary template in fastpin_base.h (#5072).
#if defined(FASTLED_FORCE_SOFTWARE_PINS)
#define _FL_MGM240_FASTPIN(PIN)
#else
#define _FL_MGM240_FASTPIN(PIN) template<> class FastPin<PIN> : public Mgm240HwPin<PIN> {};
#endif
#define _FL_DEFPIN(PIN, BIT, PORT_LETTER, MASK) \
    template<> class Mgm240HwPin<PIN> : public _ARMPIN<MASK, __generated_struct_GPIO_PORT_##PORT_LETTER, PORT_NUM_##PORT_LETTER, BIT> {}; \
    _FL_MGM240_FASTPIN(PIN)

// Define port numbers for template parameters
#define PORT_NUM_A 0
#define PORT_NUM_B 1
#define PORT_NUM_C 2
#define PORT_NUM_D 3

// Arduino pin N must be the pad that the vendor core's digitalWrite(N) drives,
// so each Silicon Labs variant gets its own table (#5104). The tables below are
// generated from each variant's gPinNames[] in Silicon Labs Arduino core 2.2.0.
#if defined(ARDUINO_BGM220EXPLORERKIT) || defined(ARDUINO_SILABS_BGM220EXPLORERKIT)
// Silicon Labs Arduino core 2.2.0, variants/bgm220explorerkit/arduino_variant.cpp gPinNames
#define FL_MGM240_PIN_TABLE_FROM_VARIANT 1
_FL_DEFPIN( 0,  0, A, (1 << 0));  // D0
_FL_DEFPIN( 1,  0, C, (1 << 0));  // D1 - SPI SDO - WU
_FL_DEFPIN( 2,  1, C, (1 << 1));  // D2 - SPI SDI
_FL_DEFPIN( 3,  2, C, (1 << 2));  // D3 - SPI SCK
_FL_DEFPIN( 4,  3, C, (1 << 3));  // D4 - SPI CS
_FL_DEFPIN( 5,  6, C, (1 << 6));  // D5
_FL_DEFPIN( 6,  0, B, (1 << 0));  // D6
_FL_DEFPIN( 7,  7, C, (1 << 7));  // A0 - Button - WU - 7
_FL_DEFPIN( 8,  4, A, (1 << 4));  // A1 - LED - 8
_FL_DEFPIN( 9,  3, D, (1 << 3));  // A2 - SDA
_FL_DEFPIN(10,  2, D, (1 << 2));  // A3 - SCL - WU
_FL_DEFPIN(11,  1, B, (1 << 1));  // A4 - Tx1 - WU - 11
_FL_DEFPIN(12,  2, B, (1 << 2));  // A5 - Rx1 - 12
_FL_DEFPIN(13,  3, B, (1 << 3));  // A6 - WU
_FL_DEFPIN(14,  4, B, (1 << 4));  // A7
_FL_DEFPIN(15,  6, A, (1 << 6));  // Rx - 15
_FL_DEFPIN(16,  5, A, (1 << 5));  // Tx - WU - 16
#elif defined(ARDUINO_EZURIO_LYRA24P20) || defined(ARDUINO_LYRA24P20)
// Silicon Labs Arduino core 2.2.0, variants/lyra24p20/arduino_variant.cpp gPinNames
#define FL_MGM240_PIN_TABLE_FROM_VARIANT 1
_FL_DEFPIN( 0,  4, A, (1 << 4));  // D0
_FL_DEFPIN( 1,  5, A, (1 << 5));  // D1 - WU
_FL_DEFPIN( 2,  4, C, (1 << 4));  // D2 - SPI SDO
_FL_DEFPIN( 3,  5, C, (1 << 5));  // D3 - SPI SDI - WU
_FL_DEFPIN( 4,  2, C, (1 << 2));  // D4 - SPI SCK
_FL_DEFPIN( 5,  3, C, (1 << 3));  // D5 - SPI CS
_FL_DEFPIN( 6,  6, C, (1 << 6));  // D6 - SPI1 SS
_FL_DEFPIN( 7,  0, B, (1 << 0));  // D7 - SPI1 SCK - DAC0
_FL_DEFPIN( 8,  6, A, (1 << 6));  // A0 - 8
_FL_DEFPIN( 9,  8, A, (1 << 8));  // A1 - LED - 9
_FL_DEFPIN(10,  3, D, (1 << 3));  // A2 - SDA - 10
_FL_DEFPIN(11,  2, D, (1 << 2));  // A3 - SCL - WU - 11
_FL_DEFPIN(12,  1, B, (1 << 1));  // A4 - Tx - DAC1 - WU - 12
_FL_DEFPIN(13,  2, B, (1 << 2));  // A5 - Rx - DAC2 - 13
_FL_DEFPIN(14,  3, B, (1 << 3));  // A6 - SPI1 SDO - Tx1 - DAC3 - WU - 14
_FL_DEFPIN(15,  4, B, (1 << 4));  // A7 - SPI1 SDI - Rx1 - 15
_FL_DEFPIN(16,  7, C, (1 << 7));  // Button - WU - 16
_FL_DEFPIN(17,  0, A, (1 << 0));  // Rx - 17
_FL_DEFPIN(18,  7, A, (1 << 7));  // Tx - 18
#elif defined(ARDUINO_NANO_MATTER)
// Silicon Labs Arduino core 2.2.0, variants/nano_matter/arduino_variant.cpp gPinNames
#define FL_MGM240_PIN_TABLE_FROM_VARIANT 1
_FL_DEFPIN( 0,  4, A, (1 << 4));  // D0 - Tx1 - SPI1 SDO
_FL_DEFPIN( 1,  5, A, (1 << 5));  // D1 - Rx1 - SPI1 SDI - WU
_FL_DEFPIN( 2,  3, A, (1 << 3));  // D2 - SPI1 SCK
_FL_DEFPIN( 3,  6, C, (1 << 6));  // D3 - SPI1 SS
_FL_DEFPIN( 4,  7, C, (1 << 7));  // D4 - SDA1 - WU
_FL_DEFPIN( 5,  8, C, (1 << 8));  // D5 - SCL1
_FL_DEFPIN( 6,  9, C, (1 << 9));  // D6
_FL_DEFPIN( 7,  2, D, (1 << 2));  // D7 - WU
_FL_DEFPIN( 8,  3, D, (1 << 3));  // D8
_FL_DEFPIN( 9,  4, D, (1 << 4));  // D9
_FL_DEFPIN(10,  5, D, (1 << 5));  // D10 - SPI SS
_FL_DEFPIN(11,  9, A, (1 << 9));  // D11 - SPI SDO
_FL_DEFPIN(12,  8, A, (1 << 8));  // D12 - SPI SDI
_FL_DEFPIN(13,  4, B, (1 << 4));  // D13 - SPI SCK
_FL_DEFPIN(14,  0, B, (1 << 0));  // A0 - DAC0
_FL_DEFPIN(15,  2, B, (1 << 2));  // A1 - DAC2
_FL_DEFPIN(16,  5, B, (1 << 5));  // A2
_FL_DEFPIN(17,  0, C, (1 << 0));  // A3 - WU
_FL_DEFPIN(18,  6, A, (1 << 6));  // A4 - SDA
_FL_DEFPIN(19,  7, A, (1 << 7));  // A5 - SCL
_FL_DEFPIN(20,  1, B, (1 << 1));  // A6 - DAC1 - WU
_FL_DEFPIN(21,  3, B, (1 << 3));  // A7 - DAC3 - WU
_FL_DEFPIN(22,  1, C, (1 << 1));  // LED R - 22
_FL_DEFPIN(23,  2, C, (1 << 2));  // LED G - 23
_FL_DEFPIN(24,  3, C, (1 << 3));  // LED B - 24
_FL_DEFPIN(25,  0, A, (1 << 0));  // Button - 25
_FL_DEFPIN(26,  4, C, (1 << 4));  // Serial Tx - 26
_FL_DEFPIN(27,  5, C, (1 << 5));  // Serial Rx - WU - 27
#elif defined(ARDUINO_SILABS_THINGPLUSMATTER) || defined(ARDUINO_THINGPLUSMATTER) || defined(ARDUINO_SPARKFUN_THINGPLUSMATTER)
// Silicon Labs Arduino core 2.2.0, variants/thingplusmatter/arduino_variant.cpp gPinNames
#define FL_MGM240_PIN_TABLE_FROM_VARIANT 1
_FL_DEFPIN( 0,  7, C, (1 << 7));  // D0 - WU
_FL_DEFPIN( 1,  5, A, (1 << 5));  // D1 - Tx - WU
_FL_DEFPIN( 2,  6, A, (1 << 6));  // D2 - Rx
_FL_DEFPIN( 3,  6, C, (1 << 6));  // D3 - SPI SDI
_FL_DEFPIN( 4,  3, C, (1 << 3));  // D4 - SPI SDO
_FL_DEFPIN( 5,  2, C, (1 << 2));  // D5 - SPI SCK
_FL_DEFPIN( 6,  1, C, (1 << 1));  // D6 - SPI SS
_FL_DEFPIN( 7,  0, C, (1 << 0));  // D7 - WU
_FL_DEFPIN( 8,  0, D, (1 << 0));  // D8
_FL_DEFPIN( 9,  1, D, (1 << 1));  // D9
_FL_DEFPIN(10,  2, D, (1 << 2));  // D10 - WU
_FL_DEFPIN(11,  3, D, (1 << 3));  // D11
_FL_DEFPIN(12,  4, B, (1 << 4));  // A0 - SDA
_FL_DEFPIN(13,  3, B, (1 << 3));  // A1 - SCL - DAC3 - WU
_FL_DEFPIN(14,  2, B, (1 << 2));  // A2 - SPI1 SDI - Rx1 - DAC2
_FL_DEFPIN(15,  1, B, (1 << 1));  // A3 - SPI1 SDO - Tx1 - DAC1 - WU
_FL_DEFPIN(16,  0, B, (1 << 0));  // A4 - SPI1 SCK - DAC0
_FL_DEFPIN(17,  0, A, (1 << 0));  // A5 - SPI1 SS
_FL_DEFPIN(18,  4, A, (1 << 4));  // A6
_FL_DEFPIN(19,  4, C, (1 << 4));  // A7
_FL_DEFPIN(20,  5, C, (1 << 5));  // A8 - WU
_FL_DEFPIN(21,  8, A, (1 << 8));  // LED - 21
_FL_DEFPIN(22,  7, A, (1 << 7));  // SD card SPI CS - 22
#elif defined(ARDUINO_SILABS_XG24DEVKIT) || defined(ARDUINO_XG24DEVKIT)
// Silicon Labs Arduino core 2.2.0, variants/xg24devkit/arduino_variant.cpp gPinNames
#define FL_MGM240_PIN_TABLE_FROM_VARIANT 1
_FL_DEFPIN( 0,  3, C, (1 << 3));  // D0 - SPI SDO
_FL_DEFPIN( 1,  2, C, (1 << 2));  // D1 - SPI SDI
_FL_DEFPIN( 2,  1, C, (1 << 1));  // D2 - SPI SCK
_FL_DEFPIN( 3,  7, A, (1 << 7));  // D3 - SPI CS
_FL_DEFPIN( 4,  5, A, (1 << 5));  // D4 - Tx - WU
_FL_DEFPIN( 5,  6, A, (1 << 6));  // D5 - Rx
_FL_DEFPIN( 6,  5, C, (1 << 5));  // D6 - SDA - WU
_FL_DEFPIN( 7,  2, B, (1 << 2));  // A0 - DAC2
_FL_DEFPIN( 8,  0, B, (1 << 0));  // A1 - DAC0
_FL_DEFPIN( 9,  3, B, (1 << 3));  // A2 - DAC3
_FL_DEFPIN(10,  2, D, (1 << 2));  // A3 - WU
_FL_DEFPIN(11,  4, C, (1 << 4));  // A4 - SCL
_FL_DEFPIN(12,  2, D, (1 << 2));  // LED R - 12
_FL_DEFPIN(13,  4, A, (1 << 4));  // LED G - 13
_FL_DEFPIN(14,  0, B, (1 << 0));  // LED B - 14
_FL_DEFPIN(15,  2, B, (1 << 2));  // Button - DAC2 - 15
_FL_DEFPIN(16,  3, B, (1 << 3));  // Button - DAC3 - WU - 16
_FL_DEFPIN(17,  9, C, (1 << 9));  // Sensor array power - 17
_FL_DEFPIN(18,  8, C, (1 << 8));  // Microphone power - 18
_FL_DEFPIN(19,  0, C, (1 << 0));  // SPI flash CS - WU - 19
_FL_DEFPIN(20,  3, D, (1 << 3));  // I2S SCK - 20
_FL_DEFPIN(21,  4, D, (1 << 4));  // I2S SD - 21
_FL_DEFPIN(22,  5, D, (1 << 5));  // I2S WS - WU - 22
#elif defined(ARDUINO_SILABS_XG24EXPLORERKIT) || defined(ARDUINO_XG24EXPLORERKIT)
// Silicon Labs Arduino core 2.2.0, variants/xg24explorerkit/arduino_variant.cpp gPinNames
#define FL_MGM240_PIN_TABLE_FROM_VARIANT 1
_FL_DEFPIN( 0,  9, C, (1 << 9));  // D0
_FL_DEFPIN( 1,  3, C, (1 << 3));  // D1 - SPI SDO
_FL_DEFPIN( 2,  2, C, (1 << 2));  // D2 - SPI SDI
_FL_DEFPIN( 3,  1, C, (1 << 1));  // D3 - SPI SCK
_FL_DEFPIN( 4,  0, C, (1 << 0));  // D4 - SPI CS - WU
_FL_DEFPIN( 5,  8, C, (1 << 8));  // D5 - SPI1 SS
_FL_DEFPIN( 6,  0, B, (1 << 0));  // D6 - SPI1 SCK - DAC0
_FL_DEFPIN( 7,  2, D, (1 << 2));  // A0 - WU
_FL_DEFPIN( 8,  3, D, (1 << 3));  // A1
_FL_DEFPIN( 9,  5, B, (1 << 5));  // A2 - SDA
_FL_DEFPIN(10,  4, B, (1 << 4));  // A3 - SCL
_FL_DEFPIN(11,  4, D, (1 << 4));  // A4 - 11
_FL_DEFPIN(12,  5, D, (1 << 5));  // A5 - 12 - WU
_FL_DEFPIN(13,  1, B, (1 << 1));  // A6 - SPI1 SDO - Tx1 - DAC1 - WU
_FL_DEFPIN(14,  0, A, (1 << 0));  // A7 - SPI1 SDI - Rx1
_FL_DEFPIN(15,  4, A, (1 << 4));  // LED - 15
_FL_DEFPIN(16,  7, A, (1 << 7));  // LED - 16
_FL_DEFPIN(17,  2, B, (1 << 2));  // Button - DAC2 - 17
_FL_DEFPIN(18,  3, B, (1 << 3));  // Button - DAC3 - WU - 18
_FL_DEFPIN(19,  4, C, (1 << 4));  // SCL1 - 19
_FL_DEFPIN(20,  5, C, (1 << 5));  // SDA1 - WU - 20
_FL_DEFPIN(21,  6, A, (1 << 6));  // Rx - 21
_FL_DEFPIN(22,  5, A, (1 << 5));  // Tx - WU - 22
#elif defined(ARDUINO_SILABS_XG27DEVKIT) || defined(ARDUINO_XG27DEVKIT)
// Silicon Labs Arduino core 2.2.0, variants/xg27devkit/arduino_variant.cpp gPinNames
#define FL_MGM240_PIN_TABLE_FROM_VARIANT 1
_FL_DEFPIN( 0,  0, C, (1 << 0));  // D0 - SPI SDO - WU
_FL_DEFPIN( 1,  1, C, (1 << 1));  // D1 - SPI SDI
_FL_DEFPIN( 2,  2, C, (1 << 2));  // D2 - SPI SCK
_FL_DEFPIN( 3,  2, B, (1 << 2));  // D3 - SPI CS
_FL_DEFPIN( 4,  5, A, (1 << 5));  // D4 - Tx - WU
_FL_DEFPIN( 5,  6, A, (1 << 6));  // D5 - Rx
_FL_DEFPIN( 6,  2, D, (1 << 2));  // D6 - SDA - WU
_FL_DEFPIN( 7,  8, A, (1 << 8));  // A0 - Tx1
_FL_DEFPIN( 8,  7, A, (1 << 7));  // A1 - Rx1
_FL_DEFPIN( 9,  0, B, (1 << 0));  // A2
_FL_DEFPIN(10,  1, B, (1 << 1));  // A3 - WU
_FL_DEFPIN(11,  4, A, (1 << 4));  // A4 - LED
_FL_DEFPIN(12,  3, B, (1 << 3));  // A5 - Button - WU
_FL_DEFPIN(13,  3, D, (1 << 3));  // A6 - SCL
_FL_DEFPIN(14,  4, A, (1 << 4));  // LED - 14
_FL_DEFPIN(15,  3, B, (1 << 3));  // Button - 15
_FL_DEFPIN(16,  6, C, (1 << 6));  // Sensor array power - 16
_FL_DEFPIN(17,  7, C, (1 << 7));  // Microphone power - WU - 17
_FL_DEFPIN(18,  4, B, (1 << 4));  // IMU power - 18
#elif defined(ARDUINO_XIAO_MG24)
// Silicon Labs Arduino core 2.2.0, variants/xiao_mg24/arduino_variant.cpp gPinNames
#define FL_MGM240_PIN_TABLE_FROM_VARIANT 1
_FL_DEFPIN( 0,  0, C, (1 << 0));  // D0 - SPI SS - WU
_FL_DEFPIN( 1,  1, C, (1 << 1));  // D1
_FL_DEFPIN( 2,  2, C, (1 << 2));  // D2
_FL_DEFPIN( 3,  3, C, (1 << 3));  // D3
_FL_DEFPIN( 4,  4, C, (1 << 4));  // D4 - SDA
_FL_DEFPIN( 5,  5, C, (1 << 5));  // D5 - SCL - WU
_FL_DEFPIN( 6,  6, C, (1 << 6));  // D6 - Serial1 Tx
_FL_DEFPIN( 7,  7, C, (1 << 7));  // D7 - Serial1 Rx - WU
_FL_DEFPIN( 8,  3, A, (1 << 3));  // D8 - SPI SCK
_FL_DEFPIN( 9,  4, A, (1 << 4));  // D9 - SPI SDI
_FL_DEFPIN(10,  5, A, (1 << 5));  // D10 - SPI SDO - WU
_FL_DEFPIN(11,  9, A, (1 << 9));  // D11 - Serial Rx - 11
_FL_DEFPIN(12,  8, A, (1 << 8));  // D12 - Serial Tx - 12
_FL_DEFPIN(13,  2, B, (1 << 2));  // D13 - SDA1 - DAC2 - 13
_FL_DEFPIN(14,  3, B, (1 << 3));  // D14 - SCL1 - DAC3 - WU - 14
_FL_DEFPIN(15,  0, B, (1 << 0));  // D15 - SPI1 SDO - DAC0 - 15
_FL_DEFPIN(16,  1, B, (1 << 1));  // D16 - SPI1 SDI - DAC1 - WU - 16
_FL_DEFPIN(17,  0, A, (1 << 0));  // D17 - SPI1 SCK - 17
_FL_DEFPIN(18,  2, D, (1 << 2));  // D18 - WU - 18
_FL_DEFPIN(19,  5, D, (1 << 5));  // IMU enable - WU - 19
_FL_DEFPIN(20,  7, A, (1 << 7));  // LED - 20
_FL_DEFPIN(21,  6, A, (1 << 6));  // Flash CS - 21
_FL_DEFPIN(22,  8, C, (1 << 8));  // MIC enable - 22
_FL_DEFPIN(23,  9, C, (1 << 9));  // MIC data - 23
_FL_DEFPIN(24,  4, D, (1 << 4));  // VBAT ADC - 24
_FL_DEFPIN(25,  3, D, (1 << 3));  // Battery charger enable - 25
_FL_DEFPIN(26,  4, B, (1 << 4));  // RF switch - 26
_FL_DEFPIN(27,  5, B, (1 << 5));  // RF switch enable - 27
#else
#warning "fastpin_arm_mgm240.h has no pin table for this Silicon Labs variant; FastPin<N> may drive a different pad than digitalWrite(N)."
// Generic fallback, not taken from any vendor variant.
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
#endif

#if defined(FASTLED_FORCE_SOFTWARE_PINS)
// The pin map then emits only Mgm240HwPin, so fl::FastPin uses the forced
// software primary template in fastpin_base.h (#5072).
#warning "Software pin support forced, pin access will be slightly slower."
#define NO_HARDWARE_PIN_SUPPORT
#undef HAS_HARDWARE_PIN_SUPPORT
#else
#define HAS_HARDWARE_PIN_SUPPORT
#endif
}  // namespace fl
