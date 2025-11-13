#pragma once

// ATmega32U4 family pin mappings
// Includes: ATmega32U4
// Used in: Arduino Leonardo, Arduino Pro Micro, Teensy 2.0

#include "platforms/avr/atmega/common/avr_pin.h"
#include "fl/fastpin_base.h"

namespace fl {

#if defined(__AVR_ATmega32U4__)

#ifndef FASTLED_FORCE_SOFTWARE_PINS

#define AVR_PIN_CYCLES(_PIN) ((((int)FastPin<_PIN>::port())-0x20 < 64) ? 1 : 2)

typedef volatile uint8_t & reg8_t;

#define _R(T) struct __gen_struct_ ## T
#define _RD8(T) struct __gen_struct_ ## T { static inline reg8_t r() { return T; }};

#define _FL_IO(L,C) _RD8(DDR ## L); _RD8(PORT ## L); _RD8(PIN ## L); _FL_DEFINE_PORT3(L, C, _R(PORT ## L));
#define _FL_DEFPIN(_PIN, BIT, L) template<> class FastPin<_PIN> : public _AVRPIN<_PIN, 1<<BIT, _R(PORT ## L), _R(DDR ## L), _R(PIN ## L)> {};

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
#ifdef PORTE
  _FL_IO(E,4)
#endif
#ifdef PORTF
  _FL_IO(F,5)
#endif

#if defined(CORE_TEENSY)
// Teensy 2.0 pin mappings
#define MAX_PIN 23
_FL_DEFPIN(0, 0, B); _FL_DEFPIN(1, 1, B); _FL_DEFPIN(2, 2, B); _FL_DEFPIN(3, 3, B);
_FL_DEFPIN(4, 7, B); _FL_DEFPIN(5, 0, D); _FL_DEFPIN(6, 1, D); _FL_DEFPIN(7, 2, D);
_FL_DEFPIN(8, 3, D); _FL_DEFPIN(9, 6, C); _FL_DEFPIN(10, 7, C); _FL_DEFPIN(11, 6, D);
_FL_DEFPIN(12, 7, D); _FL_DEFPIN(13, 4, B); _FL_DEFPIN(14, 5, B); _FL_DEFPIN(15, 6, B);
_FL_DEFPIN(16, 7, F); _FL_DEFPIN(17, 6, F); _FL_DEFPIN(18, 5, F); _FL_DEFPIN(19, 4, F);
_FL_DEFPIN(20, 1, F); _FL_DEFPIN(21, 0, F); _FL_DEFPIN(22, 4, D); _FL_DEFPIN(23, 5, D);

#define SPI_DATA 2
#define SPI_CLOCK 1
#define SPI_SELECT 0
#define AVR_HARDWARE_SPI 1
#define HAS_HARDWARE_PIN_SUPPORT 1

// PD3/PD5
#define SPI_UART1_DATA 8
#define SPI_UART1_CLOCK 23

#else
// Arduino Leonardo / Pro Micro pin mappings
#define MAX_PIN 30
_FL_DEFPIN(0, 2, D); _FL_DEFPIN(1, 3, D); _FL_DEFPIN(2, 1, D); _FL_DEFPIN(3, 0, D);
_FL_DEFPIN(4, 4, D); _FL_DEFPIN(5, 6, C); _FL_DEFPIN(6, 7, D); _FL_DEFPIN(7, 6, E);
_FL_DEFPIN(8, 4, B); _FL_DEFPIN(9, 5, B); _FL_DEFPIN(10, 6, B); _FL_DEFPIN(11, 7, B);
_FL_DEFPIN(12, 6, D); _FL_DEFPIN(13, 7, C); _FL_DEFPIN(14, 3, B); _FL_DEFPIN(15, 1, B);
_FL_DEFPIN(16, 2, B); _FL_DEFPIN(17, 0, B); _FL_DEFPIN(18, 7, F); _FL_DEFPIN(19, 6, F);
_FL_DEFPIN(20, 5, F); _FL_DEFPIN(21, 4, F); _FL_DEFPIN(22, 1, F); _FL_DEFPIN(23, 0, F);
_FL_DEFPIN(24, 4, D); _FL_DEFPIN(25, 7, D); _FL_DEFPIN(26, 4, B); _FL_DEFPIN(27, 5, B);
_FL_DEFPIN(28, 6, B); _FL_DEFPIN(29, 6, D); _FL_DEFPIN(30, 5, D);

#define SPI_DATA 16
#define SPI_CLOCK 15
#define AVR_HARDWARE_SPI 1
#define HAS_HARDWARE_PIN_SUPPORT 1

// PD3/PD5
#define SPI_UART1_DATA 1
#define SPI_UART1_CLOCK 30

#endif // CORE_TEENSY

#endif // FASTLED_FORCE_SOFTWARE_PINS

#endif // ATmega32U4 check

}  // namespace fl
