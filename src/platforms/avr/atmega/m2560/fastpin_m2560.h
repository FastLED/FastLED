#pragma once

// ATmega2560 family pin mappings
// Includes: ATmega2560, ATmega1280
// Used in: Arduino MEGA 2560

#include "platforms/avr/atmega/common/avr_pin.h"
#include "fl/fastpin_base.h"

namespace fl {

#if defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)

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
#ifdef PORTG
  _FL_IO(G,6)
#endif
#ifdef PORTH
  _FL_IO(H,7)
#endif
#ifdef PORTJ
  _FL_IO(J,9)
#endif
#ifdef PORTK
  _FL_IO(K,10)
#endif
#ifdef PORTL
  _FL_IO(L,11)
#endif

// Arduino MEGA 2560 - 70 digital pins
#define MAX_PIN 69
_FL_DEFPIN(0, 0, E); _FL_DEFPIN(1, 1, E); _FL_DEFPIN(2, 4, E); _FL_DEFPIN(3, 5, E);
_FL_DEFPIN(4, 5, G); _FL_DEFPIN(5, 3, E); _FL_DEFPIN(6, 3, H); _FL_DEFPIN(7, 4, H);
_FL_DEFPIN(8, 5, H); _FL_DEFPIN(9, 6, H); _FL_DEFPIN(10, 4, B); _FL_DEFPIN(11, 5, B);
_FL_DEFPIN(12, 6, B); _FL_DEFPIN(13, 7, B); _FL_DEFPIN(14, 1, J); _FL_DEFPIN(15, 0, J);
_FL_DEFPIN(16, 1, H); _FL_DEFPIN(17, 0, H); _FL_DEFPIN(18, 3, D); _FL_DEFPIN(19, 2, D);
_FL_DEFPIN(20, 1, D); _FL_DEFPIN(21, 0, D); _FL_DEFPIN(22, 0, A); _FL_DEFPIN(23, 1, A);
_FL_DEFPIN(24, 2, A); _FL_DEFPIN(25, 3, A); _FL_DEFPIN(26, 4, A); _FL_DEFPIN(27, 5, A);
_FL_DEFPIN(28, 6, A); _FL_DEFPIN(29, 7, A); _FL_DEFPIN(30, 7, C); _FL_DEFPIN(31, 6, C);
_FL_DEFPIN(32, 5, C); _FL_DEFPIN(33, 4, C); _FL_DEFPIN(34, 3, C); _FL_DEFPIN(35, 2, C);
_FL_DEFPIN(36, 1, C); _FL_DEFPIN(37, 0, C); _FL_DEFPIN(38, 7, D); _FL_DEFPIN(39, 2, G);
_FL_DEFPIN(40, 1, G); _FL_DEFPIN(41, 0, G); _FL_DEFPIN(42, 7, L); _FL_DEFPIN(43, 6, L);
_FL_DEFPIN(44, 5, L); _FL_DEFPIN(45, 4, L); _FL_DEFPIN(46, 3, L); _FL_DEFPIN(47, 2, L);
_FL_DEFPIN(48, 1, L); _FL_DEFPIN(49, 0, L); _FL_DEFPIN(50, 3, B); _FL_DEFPIN(51, 2, B);
_FL_DEFPIN(52, 1, B); _FL_DEFPIN(53, 0, B); _FL_DEFPIN(54, 0, F); _FL_DEFPIN(55, 1, F);
_FL_DEFPIN(56, 2, F); _FL_DEFPIN(57, 3, F); _FL_DEFPIN(58, 4, F); _FL_DEFPIN(59, 5, F);
_FL_DEFPIN(60, 6, F); _FL_DEFPIN(61, 7, F); _FL_DEFPIN(62, 0, K); _FL_DEFPIN(63, 1, K);
_FL_DEFPIN(64, 2, K); _FL_DEFPIN(65, 3, K); _FL_DEFPIN(66, 4, K); _FL_DEFPIN(67, 5, K);
_FL_DEFPIN(68, 6, K); _FL_DEFPIN(69, 7, K);

#define SPI_DATA 51
#define SPI_CLOCK 52
#define SPI_SELECT 53
#define AVR_HARDWARE_SPI 1
#define HAS_HARDWARE_PIN_SUPPORT 1

#endif // FASTLED_FORCE_SOFTWARE_PINS

#endif // ATmega2560 family check

}  // namespace fl
