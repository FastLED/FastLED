#pragma once

#include "platforms/avr/is_avr.h"

// ATmega328P family pin mappings
// Includes: ATmega328P, ATmega328PB, ATmega328, ATmega168P, ATmega168, ATmega8, ATmega8A
// Used in: Arduino UNO, Arduino Nano, Arduino Pro Mini

#include "platforms/avr/atmega/common/avr_pin.h"
#include "fl/fastpin_base.h"

namespace fl {

#ifdef FL_IS_AVR_ATMEGA_328P

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

#define MAX_PIN 19
_FL_DEFPIN( 0, 0, D); _FL_DEFPIN( 1, 1, D); _FL_DEFPIN( 2, 2, D); _FL_DEFPIN( 3, 3, D);
_FL_DEFPIN( 4, 4, D); _FL_DEFPIN( 5, 5, D); _FL_DEFPIN( 6, 6, D); _FL_DEFPIN( 7, 7, D);
_FL_DEFPIN( 8, 0, B); _FL_DEFPIN( 9, 1, B); _FL_DEFPIN(10, 2, B); _FL_DEFPIN(11, 3, B);
_FL_DEFPIN(12, 4, B); _FL_DEFPIN(13, 5, B); _FL_DEFPIN(14, 0, C); _FL_DEFPIN(15, 1, C);
_FL_DEFPIN(16, 2, C); _FL_DEFPIN(17, 3, C); _FL_DEFPIN(18, 4, C); _FL_DEFPIN(19, 5, C);

#define SPI_DATA 11
#define SPI_CLOCK 13
#define SPI_SELECT 10
#define AVR_HARDWARE_SPI 1
#define HAS_HARDWARE_PIN_SUPPORT 1

#if !defined(__AVR_ATmega8__) && !defined(__AVR_ATmega8A__)
#define SPI_UART0_DATA 1
#define SPI_UART0_CLOCK 4
#endif

#endif // FASTLED_FORCE_SOFTWARE_PINS

#endif // ATmega328P family check

}  // namespace fl
