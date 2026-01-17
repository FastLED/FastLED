#pragma once

// ATtiny family pin mappings
// Includes: Classic ATtiny (25/45/85, 24/44/84, 13, 4313, 48/88, 841/441, 167/87)
//           Modern tinyAVR (202, 204, 212, 214, 402, 404, 406, 407, 412, 414, 416, 417)
//           Modern tinyAVR series (xy2, xy4, xy6, xy7)
//           Special boards: Digispark, Digispark Pro, LightBlue Bean

#include "platforms/avr/atmega/common/avr_pin.h"
#include "fl/fastpin_base.h"

namespace fl {

#ifndef FASTLED_FORCE_SOFTWARE_PINS

#define AVR_PIN_CYCLES(_PIN) ((((int)FastPin<_PIN>::port())-0x20 < 64) ? 1 : 2)

typedef volatile uint8_t & reg8_t;

#define _R(T) struct __gen_struct_ ## T
#define _RD8(T) struct __gen_struct_ ## T { static inline reg8_t r() { return T; }};

// Modern ATtinyxy series uses VPORT-style registers
#if defined(__AVR_ATtinyxy7__) || defined(__AVR_ATtinyxy6__) || defined(__AVR_ATtinyxy4__) || defined(__AVR_ATtinyxy2__) || \
    defined(__AVR_ATtiny1604__) || defined(__AVR_ATtiny1616__)
// ATtiny series 0/1 with VPORT registers (ATtiny 0/1/2-series)
#define _FL_IO(L,C) _RD8(PORT ## L ## _DIR); _RD8(PORT ## L ## _OUT); _RD8(PORT ## L ## _IN); _FL_DEFINE_PORT3(L, C, _R(PORT ## L ## _OUT));
#define _FL_DEFPIN(_PIN, BIT, L) template<> class FastPin<_PIN> : public _AVRPIN<_PIN, 1<<BIT, _R(PORT ## L ## _OUT), _R(PORT ## L ## _DIR), _R(PORT ## L ## _IN)> {};
#else
// Classic ATtiny uses DDR/PORT/PIN registers
#define _FL_IO(L,C) _RD8(DDR ## L); _RD8(PORT ## L); _RD8(PIN ## L); _FL_DEFINE_PORT3(L, C, _R(PORT ## L));
#define _FL_DEFPIN(_PIN, BIT, L) template<> class FastPin<_PIN> : public _AVRPIN<_PIN, 1<<BIT, _R(PORT ## L), _R(DDR ## L), _R(PIN ## L)> {};
#endif

// Pre-do all the port definitions
#ifdef PORTA
  _FL_IO(A,0)
#endif
#ifdef PORTB
  _FL_IO(B,1)
#endif
#ifdef PORTC
  _FL_IO(C,2)
#endif
#ifdef PORTD
  _FL_IO(D,3)
#endif

#if defined(__AVR_ATtiny85__) || defined(__AVR_ATtiny45__) || defined(__AVR_ATtiny25__)

#if defined(__AVR_ATtiny25__)
#pragma message "ATtiny25 has very limited storage. This library could use up to more than 100% of its flash size"
#endif

#define MAX_PIN 5

_FL_DEFPIN(0, 0, B); _FL_DEFPIN(1, 1, B); _FL_DEFPIN(2, 2, B); _FL_DEFPIN(3, 3, B);
_FL_DEFPIN(4, 4, B); _FL_DEFPIN(5, 5, B);

#define HAS_HARDWARE_PIN_SUPPORT 1

#elif defined(__AVR_ATtiny4313__)

#define NEED_CXX_BITS 1

// Note, still work in progress, we fail with:
// C:\Users\niteris\AppData\Local\Temp\ccGpqhqm.s:1422: Error: illegal opcode or for mcu avr25
// lto-wrapper.exe: fatal error: avr-g++ returned 1 exit status
// compilation terminated.
// c:/users/niteris/.platformio/packages/toolchain-atmelavr/bin/../lib/gcc/avr/7.3.0/../../../../avr/bin/ld.exe: error: lto-wrapper failed
// collect2.exe: error: ld returned 1 exit status
// *** [.pio\build\attiny4313\firmware.elf] Error 1

#define MAX_PIN 19

_FL_DEFPIN(0, 0, A); // PA0 (ADC0/PCINT0)
_FL_DEFPIN(1, 1, A); // PA1 (ADC1/PCINT1)
_FL_DEFPIN(2, 2, A); // PA2 (ADC2/PCINT2)

_FL_DEFPIN(3, 0, D); // PD0 (RXD/PCINT16)
_FL_DEFPIN(4, 1, D); // PD1 (TXD/PCINT17)
_FL_DEFPIN(5, 2, D); // PD2 (INT0/PCINT18)
_FL_DEFPIN(6, 3, D); // PD3 (INT1/PCINT19)
_FL_DEFPIN(7, 4, D); // PD4 (T0/XCK/PCINT20)
_FL_DEFPIN(8, 5, D); // PD5 (T1/PCINT21)
_FL_DEFPIN(9, 6, D); // PD6 (AIN0/PCINT22)

_FL_DEFPIN(11, 0, B); // PB0 (ICP/PCINT8)
_FL_DEFPIN(12, 1, B); // PB1 (OC0A/PCINT9)
_FL_DEFPIN(13, 2, B); // PB2 (SS/OC0B/PCINT10)
_FL_DEFPIN(14, 3, B); // PB3 (MOSI/OC1A/PCINT11)
_FL_DEFPIN(15, 4, B); // PB4 (MISO/OC1B/PCINT12)
_FL_DEFPIN(16, 5, B); // PB5 (SCK/PCINT13)
_FL_DEFPIN(17, 6, B); // PB6 (XTAL1/PCINT14)
_FL_DEFPIN(18, 7, B); // PB7 (XTAL2/PCINT15)

#define HAS_HARDWARE_PIN_SUPPORT 1

#elif defined(__AVR_ATtiny13__)

#define NEED_CXX_BITS 1
#define MAX_PIN 5

// We are still getting this weird error:
// C:\Users\niteris\AppData\Local\Temp\ccojfQbm.s: Assembler messages:
// C:\Users\niteris\AppData\Local\Temp\ccojfQbm.s:469: Error: illegal opcode or for mcu avr25
// C:\Users\niteris\AppData\Local\Temp\ccojfQbm.s:1505: Error: illegal opcode or for mcu avr25
// C:\Users\niteris\AppData\Local\Temp\ccojfQbm.s:1508: Error: illegal opcode or for mcu avr25
// C:\Users\niteris\AppData\Local\Temp\ccojfQbm.s:1560: Error: illegal opcode or for mcu avr25
// C:\Users\niteris\AppData\Local\Temp\ccojfQbm.s:1563: Error: illegal opcode or for mcu avr25
// lto-wrapper.exe: fatal error: avr-g++ returned 1 exit status
// compilation terminated.
// c:/users/niteris/.platformio/packages/toolchain-atmelavr/bin/../lib/gcc/avr/7.3.0/../../../../avr/bin/ld.exe: error: lto-wrapper failed
// collect2.exe: error: ld returned 1 exit status

_FL_DEFPIN(0, 0, B); // PB0 (MOSI/AIN0/OC0A/PCINT0)
_FL_DEFPIN(1, 1, B); // PB1 (MISO/AIN1/OC0B/INT0/PCINT1)
_FL_DEFPIN(2, 2, B); // PB2 (SCK/ADC1/T0/PCINT2)
_FL_DEFPIN(3, 3, B); // PB3 (PCINT3/CLKI/ADC3)
_FL_DEFPIN(4, 4, B); // PB4 (PCINT4/ADC2)
_FL_DEFPIN(5, 5, B); // PB5 (PCINT5/RESET/ADC0/dW)

#define HAS_HARDWARE_PIN_SUPPORT 1

#elif defined(__AVR_ATtiny48__) || defined(__AVR_ATtiny88__)

#define MAX_PIN 25
_FL_DEFPIN( 0, 0, D); _FL_DEFPIN( 1, 1, D); _FL_DEFPIN( 2, 2, D); _FL_DEFPIN( 3, 3, D);
_FL_DEFPIN( 4, 4, D); _FL_DEFPIN( 5, 5, D); _FL_DEFPIN( 6, 6, D); _FL_DEFPIN( 7, 7, D);
_FL_DEFPIN( 8, 0, B); _FL_DEFPIN( 9, 1, B); _FL_DEFPIN(10, 2, B); _FL_DEFPIN(11, 3, B);
_FL_DEFPIN(12, 4, B); _FL_DEFPIN(13, 5, B); _FL_DEFPIN(14, 7, B); _FL_DEFPIN(15, 2, A);
_FL_DEFPIN(16, 3, A); _FL_DEFPIN(17, 0, A); _FL_DEFPIN(18, 1, A); _FL_DEFPIN(19, 0, C);
_FL_DEFPIN(20, 1, C); _FL_DEFPIN(21, 2, C); _FL_DEFPIN(22, 3, C); _FL_DEFPIN(23, 4, C);
_FL_DEFPIN(24, 5, C); _FL_DEFPIN(25, 7, C);

#define SPI_DATA 11
#define SPI_CLOCK 13
#define SPI_SELECT 10
#define AVR_HARDWARE_SPI 1
#define HAS_HARDWARE_PIN_SUPPORT 1

#elif defined(__AVR_ATtiny841__) || defined(__AVR_ATtiny441__)
#define MAX_PIN 11

_FL_DEFPIN(0, 0, B); _FL_DEFPIN(1, 1, B); _FL_DEFPIN(2, 2, B);
_FL_DEFPIN(3, 7, A); _FL_DEFPIN(4, 6, A); _FL_DEFPIN(5, 5, A);
_FL_DEFPIN(6, 4, A); _FL_DEFPIN(7, 3, A); _FL_DEFPIN(8, 2, A);
_FL_DEFPIN(9, 1, A); _FL_DEFPIN(10, 0, A); _FL_DEFPIN(11, 3, B);

#define HAS_HARDWARE_PIN_SUPPORT 1

#elif defined(ARDUINO_AVR_DIGISPARK) // digispark pin layout
#define MAX_PIN 5
#define HAS_HARDWARE_PIN_SUPPORT 1

_FL_DEFPIN(0, 0, B); _FL_DEFPIN(1, 1, B); _FL_DEFPIN(2, 2, B);
_FL_DEFPIN(3, 7, A); _FL_DEFPIN(4, 6, A); _FL_DEFPIN(5, 5, A);

#elif defined(__AVR_ATtiny24__) || defined(__AVR_ATtiny44__) || defined(__AVR_ATtiny84__)

#define MAX_PIN 10

_FL_DEFPIN(0, 0, A); _FL_DEFPIN(1, 1, A); _FL_DEFPIN(2, 2, A); _FL_DEFPIN(3, 3, A);
_FL_DEFPIN(4, 4, A); _FL_DEFPIN(5, 5, A); _FL_DEFPIN(6, 6, A); _FL_DEFPIN(7, 7, A);
_FL_DEFPIN(8, 2, B); _FL_DEFPIN(9, 1, B); _FL_DEFPIN(10, 0, B);

#define HAS_HARDWARE_PIN_SUPPORT 1

#elif defined(ARDUINO_AVR_DIGISPARKPRO)

#define MAX_PIN 12

_FL_DEFPIN(0, 0, B); _FL_DEFPIN(1, 1, B); _FL_DEFPIN(2, 2, B); _FL_DEFPIN(3, 5, B);
_FL_DEFPIN(4, 3, B); _FL_DEFPIN(5, 7, A); _FL_DEFPIN(6, 0, A); _FL_DEFPIN(7, 1, A);
_FL_DEFPIN(8, 2, A); _FL_DEFPIN(9, 3, A); _FL_DEFPIN(10, 4, A); _FL_DEFPIN(11, 5, A);
_FL_DEFPIN(12, 6, A);

#elif defined(__AVR_ATtiny167__) || defined(__AVR_ATtiny87__)

#define MAX_PIN 15

_FL_DEFPIN(0, 0, A);  _FL_DEFPIN(1, 1, A);   _FL_DEFPIN(2, 2, A);  _FL_DEFPIN(3, 3, A);
_FL_DEFPIN(4, 4, A);  _FL_DEFPIN(5, 5, A);   _FL_DEFPIN(6, 6, A);  _FL_DEFPIN(7, 7, A);
_FL_DEFPIN(8, 0, B);  _FL_DEFPIN(9, 1, B);   _FL_DEFPIN(10, 2, B); _FL_DEFPIN(11, 3, B);
_FL_DEFPIN(12, 4, B); _FL_DEFPIN(13, 5, B); _FL_DEFPIN(14, 6, B); _FL_DEFPIN(15, 7, B);

#define SPI_DATA 4
#define SPI_CLOCK 5
#define AVR_HARDWARE_SPI 1

#define HAS_HARDWARE_PIN_SUPPORT 1

#elif defined(IS_BEAN)

#define MAX_PIN 19
_FL_DEFPIN( 0, 6, D); _FL_DEFPIN( 1, 1, B); _FL_DEFPIN( 2, 2, B); _FL_DEFPIN( 3, 3, B);
_FL_DEFPIN( 4, 4, B); _FL_DEFPIN( 5, 5, B); _FL_DEFPIN( 6, 0, D); _FL_DEFPIN( 7, 7, D);
_FL_DEFPIN( 8, 0, B); _FL_DEFPIN( 9, 1, D); _FL_DEFPIN(10, 2, D); _FL_DEFPIN(11, 3, D);
_FL_DEFPIN(12, 4, D); _FL_DEFPIN(13, 5, D); _FL_DEFPIN(14, 0, C); _FL_DEFPIN(15, 1, C);
_FL_DEFPIN(16, 2, C); _FL_DEFPIN(17, 3, C); _FL_DEFPIN(18, 4, C); _FL_DEFPIN(19, 5, C);

#define SPI_DATA 3
#define SPI_CLOCK 5
#define SPI_SELECT 2
#define AVR_HARDWARE_SPI 1
#define HAS_HARDWARE_PIN_SUPPORT 1

#ifndef __AVR_ATmega8__
#define SPI_UART0_DATA 9
#define SPI_UART0_CLOCK 12
#endif

#elif defined(__AVR_ATtiny202__) || defined(__AVR_ATtiny204__) || defined(__AVR_ATtiny212__) || defined(__AVR_ATtiny214__)  || defined(__AVR_ATtiny402__) || defined(__AVR_ATtiny404__) || defined(__AVR_ATtiny406__) || defined(__AVR_ATtiny407__) || defined(__AVR_ATtiny412__) || defined(__AVR_ATtiny414__) || defined(__AVR_ATtiny416__) || defined(__AVR_ATtiny417__)
#pragma message "ATtiny2YZ or ATtiny4YZ have very limited storage. This library could use up to more than 100% of its flash size"

#elif defined(__AVR_ATtinyxy4__) || defined(__AVR_ATtiny1604__) || defined(__AVR_ATtiny804__) || defined(__AVR_ATtiny404__)
#define MAX_PIN 12
_FL_DEFPIN( 0, 4, A); _FL_DEFPIN( 1, 5, A); _FL_DEFPIN( 2, 6, A); _FL_DEFPIN( 3, 7, A);
_FL_DEFPIN( 4, 3, B); _FL_DEFPIN( 5, 2, B); _FL_DEFPIN( 6, 1, B); _FL_DEFPIN( 7, 0, B);
_FL_DEFPIN( 8, 1, A); _FL_DEFPIN( 9, 2, A); _FL_DEFPIN(10, 3, A); _FL_DEFPIN(11, 0, A);

// SPI pins: MOSI=PA1(8), MISO=PA2(9), SCK=PA3(10), SS=PA4(0)
#define SPI_DATA 8
#define SPI_CLOCK 10
#define SPI_SELECT 0
#define AVR_HARDWARE_SPI 1
#define HAS_HARDWARE_PIN_SUPPORT 1

#elif defined(__AVR_ATtinyxy6__) || defined(__AVR_ATtiny1616__) || defined(__AVR_ATtiny816__) || defined(__AVR_ATtiny416__) || defined(__AVR_ATtiny3216__)

#define MAX_PIN 18
_FL_DEFPIN( 0, 4, A); _FL_DEFPIN( 1, 5, A); _FL_DEFPIN( 2, 6, A); _FL_DEFPIN( 3, 7, A);
_FL_DEFPIN( 4, 5, B); _FL_DEFPIN( 5, 4, B); _FL_DEFPIN( 6, 3, B); _FL_DEFPIN( 7, 2, B);
_FL_DEFPIN( 8, 1, B); _FL_DEFPIN( 9, 0, B); _FL_DEFPIN(10, 0, C); _FL_DEFPIN(11, 1, C);
_FL_DEFPIN(12, 2, C); _FL_DEFPIN(13, 3, C); _FL_DEFPIN(14, 1, A); _FL_DEFPIN(15, 2, A);
_FL_DEFPIN(16, 3, A); _FL_DEFPIN(17, 0, A);

// SPI pins: MOSI=PA1(14), MISO=PA2(15), SCK=PA3(16), SS=PA4(0)
#define SPI_DATA 14
#define SPI_CLOCK 16
#define SPI_SELECT 0
#define AVR_HARDWARE_SPI 1
#define HAS_HARDWARE_PIN_SUPPORT 1

#elif defined(__AVR_ATtinyxy7__) || defined(__AVR_ATtiny1617__) || defined(__AVR_ATtiny817__) || defined(__AVR_ATtiny417__) || defined(__AVR_ATtiny3217__)

#define MAX_PIN 22
_FL_DEFPIN( 0, 4, A); _FL_DEFPIN( 1, 5, A); _FL_DEFPIN( 2, 6, A); _FL_DEFPIN( 3, 7, A);
_FL_DEFPIN( 4, 7, B); _FL_DEFPIN( 5, 6, B); _FL_DEFPIN( 6, 5, B); _FL_DEFPIN( 7, 4, B);
_FL_DEFPIN( 8, 3, B); _FL_DEFPIN( 9, 2, B); _FL_DEFPIN(10, 1, B); _FL_DEFPIN(11, 0, B);
_FL_DEFPIN(12, 0, C); _FL_DEFPIN(13, 1, C); _FL_DEFPIN(14, 2, C); _FL_DEFPIN(15, 3, C);
_FL_DEFPIN(16, 4, C); _FL_DEFPIN(17, 5, C); _FL_DEFPIN(18, 1, A); _FL_DEFPIN(19, 2, A);
_FL_DEFPIN(20, 3, A); _FL_DEFPIN(21, 0, A);

// SPI pins: MOSI=PA1(18), MISO=PA2(19), SCK=PA3(20), SS=PA4(0)
#define SPI_DATA 18
#define SPI_CLOCK 20
#define SPI_SELECT 0
#define AVR_HARDWARE_SPI 1
#define HAS_HARDWARE_PIN_SUPPORT 1

#endif

#endif // FASTLED_FORCE_SOFTWARE_PINS

}  // namespace fl
