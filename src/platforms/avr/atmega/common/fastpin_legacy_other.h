#pragma once

// Other ATmega variants pin mappings
// Includes: ATmega1284P, ATmega644P, ATmega32, ATmega16, AT90USB646, AT90USB1286,
//           ATmega32U2, ATmega16U2, ATmega8U2, AT90USB82, AT90USB162,
//           ATmega128RFA1, ATmega256RFR2, ATmega128

#include "avr_pin.h"
#include "fl/fastpin_base.h"

namespace fl {

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

#if defined(__AVR_ATmega1284__) || defined(__AVR_ATmega1284P__) || defined(__AVR_ATmega644P__) || defined(__AVR_ATmega32__) || defined(__AVR_ATmega16__)

#define MAX_PIN 31
_FL_DEFPIN(0, 0, B); _FL_DEFPIN(1, 1, B); _FL_DEFPIN(2, 2, B); _FL_DEFPIN(3, 3, B);
_FL_DEFPIN(4, 4, B); _FL_DEFPIN(5, 5, B); _FL_DEFPIN(6, 6, B); _FL_DEFPIN(7, 7, B);
_FL_DEFPIN(8, 0, D); _FL_DEFPIN(9, 1, D); _FL_DEFPIN(10, 2, D); _FL_DEFPIN(11, 3, D);
_FL_DEFPIN(12, 4, D); _FL_DEFPIN(13, 5, D); _FL_DEFPIN(14, 6, D); _FL_DEFPIN(15, 7, D);
_FL_DEFPIN(16, 0, C); _FL_DEFPIN(17, 1, C); _FL_DEFPIN(18, 2, C); _FL_DEFPIN(19, 3, C);
_FL_DEFPIN(20, 4, C); _FL_DEFPIN(21, 5, C); _FL_DEFPIN(22, 6, C); _FL_DEFPIN(23, 7, C);
_FL_DEFPIN(24, 0, A); _FL_DEFPIN(25, 1, A); _FL_DEFPIN(26, 2, A); _FL_DEFPIN(27, 3, A);
_FL_DEFPIN(28, 4, A); _FL_DEFPIN(29, 5, A); _FL_DEFPIN(30, 6, A); _FL_DEFPIN(31, 7, A);

#define SPI_DATA 5
#define SPI_CLOCK 7
#define SPI_SELECT 4
#define AVR_HARDWARE_SPI 1
#define HAS_HARDWARE_PIN_SUPPORT 1

#elif  defined(__AVR_ATmega128RFA1__) || defined(__AVR_ATmega256RFR2__)

// AKA the Pinoccio
_FL_DEFPIN( 0, 0, E); _FL_DEFPIN( 1, 1, E); _FL_DEFPIN( 2, 7, B); _FL_DEFPIN( 3, 3, E);
_FL_DEFPIN( 4, 4, E); _FL_DEFPIN( 5, 5, E); _FL_DEFPIN( 6, 2, E); _FL_DEFPIN( 7, 6, E);
_FL_DEFPIN( 8, 5, D); _FL_DEFPIN( 9, 0, B); _FL_DEFPIN(10, 2, B); _FL_DEFPIN(11, 3, B);
_FL_DEFPIN(12, 1, B); _FL_DEFPIN(13, 2, D); _FL_DEFPIN(14, 3, D); _FL_DEFPIN(15, 0, D);
_FL_DEFPIN(16, 1, D); _FL_DEFPIN(17, 4, D); _FL_DEFPIN(18, 7, E); _FL_DEFPIN(19, 6, D);
_FL_DEFPIN(20, 7, D); _FL_DEFPIN(21, 4, B); _FL_DEFPIN(22, 5, B); _FL_DEFPIN(23, 6, B);
_FL_DEFPIN(24, 0, F); _FL_DEFPIN(25, 1, F); _FL_DEFPIN(26, 2, F); _FL_DEFPIN(27, 3, F);
_FL_DEFPIN(28, 4, F); _FL_DEFPIN(29, 5, F); _FL_DEFPIN(30, 6, F); _FL_DEFPIN(31, 7, F);

#define SPI_DATA 10
#define SPI_CLOCK 12
#define SPI_SELECT 9

#define AVR_HARDWARE_SPI 1
#define HAS_HARDWARE_PIN_SUPPORT 1

#elif defined(__AVR_AT90USB646__) || defined(__AVR_AT90USB1286__)
// teensy++ 2 defs
#define MAX_PIN 45
_FL_DEFPIN(0, 0, D); _FL_DEFPIN(1, 1, D); _FL_DEFPIN(2, 2, D); _FL_DEFPIN(3, 3, D);
_FL_DEFPIN(4, 4, D); _FL_DEFPIN(5, 5, D); _FL_DEFPIN(6, 6, D); _FL_DEFPIN(7, 7, D);
_FL_DEFPIN(8, 0, E); _FL_DEFPIN(9, 1, E); _FL_DEFPIN(10, 0, C); _FL_DEFPIN(11, 1, C);
_FL_DEFPIN(12, 2, C); _FL_DEFPIN(13, 3, C); _FL_DEFPIN(14, 4, C); _FL_DEFPIN(15, 5, C);
_FL_DEFPIN(16, 6, C); _FL_DEFPIN(17, 7, C); _FL_DEFPIN(18, 6, E); _FL_DEFPIN(19, 7, E);
_FL_DEFPIN(20, 0, B); _FL_DEFPIN(21, 1, B); _FL_DEFPIN(22, 2, B); _FL_DEFPIN(23, 3, B);
_FL_DEFPIN(24, 4, B); _FL_DEFPIN(25, 5, B); _FL_DEFPIN(26, 6, B); _FL_DEFPIN(27, 7, B);
_FL_DEFPIN(28, 0, A); _FL_DEFPIN(29, 1, A); _FL_DEFPIN(30, 2, A); _FL_DEFPIN(31, 3, A);
_FL_DEFPIN(32, 4, A); _FL_DEFPIN(33, 5, A); _FL_DEFPIN(34, 6, A); _FL_DEFPIN(35, 7, A);
_FL_DEFPIN(36, 4, E); _FL_DEFPIN(37, 5, E); _FL_DEFPIN(38, 0, F); _FL_DEFPIN(39, 1, F);
_FL_DEFPIN(40, 2, F); _FL_DEFPIN(41, 3, F); _FL_DEFPIN(42, 4, F); _FL_DEFPIN(43, 5, F);
_FL_DEFPIN(44, 6, F); _FL_DEFPIN(45, 7, F);

#define SPI_DATA 22
#define SPI_CLOCK 21
#define SPI_SELECT 20
#define AVR_HARDWARE_SPI 1
#define HAS_HARDWARE_PIN_SUPPORT 1

// PD3/PD5
#define SPI_UART1_DATA 3
#define SPI_UART1_CLOCK 5

#elif defined(ARDUINO_HOODLOADER2) && (defined(__AVR_ATmega32U2__) || defined(__AVR_ATmega16U2__) || defined(__AVR_ATmega8U2__)) || defined(__AVR_AT90USB82__) || defined(__AVR_AT90USB162__)

#define MAX_PIN 20

_FL_DEFPIN( 0, 0, B); _FL_DEFPIN( 1, 1, B); _FL_DEFPIN( 2, 2, B); _FL_DEFPIN( 3, 3, B);
_FL_DEFPIN( 4, 4, B); _FL_DEFPIN( 5, 5, B); _FL_DEFPIN( 6, 6, B); _FL_DEFPIN( 7, 7, B);

_FL_DEFPIN( 8, 7, C); _FL_DEFPIN( 9, 6, C); _FL_DEFPIN( 10, 5,C); _FL_DEFPIN( 11, 4, C);
_FL_DEFPIN( 12, 2, C); _FL_DEFPIN( 13, 0, D); _FL_DEFPIN( 14, 1, D); _FL_DEFPIN(15, 2, D);
_FL_DEFPIN( 16, 3, D); _FL_DEFPIN( 17, 4, D); _FL_DEFPIN( 18, 5, D); _FL_DEFPIN( 19, 6, D);
_FL_DEFPIN( 20, 7, D);

#define HAS_HARDWARE_PIN_SUPPORT 1
// #define SPI_DATA 2
// #define SPI_CLOCK 1
// #define AVR_HARDWARE_SPI 1

#elif defined(__AVR_ATmega128__)

// FROM: https://github.com/FastLED/FastLED/issues/1223 @eag77
#define MAX_PIN 52
_FL_DEFPIN( 0, 0, E); _FL_DEFPIN( 1, 1, E); _FL_DEFPIN( 2, 2, E); _FL_DEFPIN( 3, 3, E);
_FL_DEFPIN( 4, 4, E); _FL_DEFPIN( 5, 5, E); _FL_DEFPIN( 6, 6, E); _FL_DEFPIN( 7, 7, E);
_FL_DEFPIN( 8, 0, B); _FL_DEFPIN( 9, 1, B); _FL_DEFPIN(10, 2, B); _FL_DEFPIN(11, 3, B);
_FL_DEFPIN(12, 4, B); _FL_DEFPIN(13, 5, B); _FL_DEFPIN(14, 6, B); _FL_DEFPIN(15, 7, B);
_FL_DEFPIN(16, 3, G); _FL_DEFPIN(17, 4, G); _FL_DEFPIN(18, 0, D); _FL_DEFPIN(19, 1, D);
_FL_DEFPIN(20, 2, D); _FL_DEFPIN(21, 3, D); _FL_DEFPIN(22, 4, D); _FL_DEFPIN(23, 5, D);
_FL_DEFPIN(24, 6, D); _FL_DEFPIN(25, 7, D); _FL_DEFPIN(26, 0, G); _FL_DEFPIN(27, 1, G);
_FL_DEFPIN(28, 0, C); _FL_DEFPIN(29, 1, C); _FL_DEFPIN(30, 2, C); _FL_DEFPIN(31, 3, C);
_FL_DEFPIN(32, 4, C); _FL_DEFPIN(33, 5, C); _FL_DEFPIN(34, 6, C); _FL_DEFPIN(35, 7, C);
_FL_DEFPIN(36, 2, G); _FL_DEFPIN(37, 7, A); _FL_DEFPIN(38, 6, A); _FL_DEFPIN(39, 5, A);
_FL_DEFPIN(40, 4, A); _FL_DEFPIN(41, 3, A); _FL_DEFPIN(42, 2, A); _FL_DEFPIN(43, 1, A);
_FL_DEFPIN(44, 0, A); _FL_DEFPIN(45, 0, F); _FL_DEFPIN(46, 1, F); _FL_DEFPIN(47, 2, F);
_FL_DEFPIN(48, 3, F); _FL_DEFPIN(49, 4, F); _FL_DEFPIN(50, 5, F); _FL_DEFPIN(51, 6, F);
_FL_DEFPIN(52, 7, F);

#define SPI_DATA 10
#define SPI_CLOCK 9
#define SPI_SELECT 8

#define AVR_HARDWARE_SPI 1
#define HAS_HARDWARE_PIN_SUPPORT 1

#endif

#endif // FASTLED_FORCE_SOFTWARE_PINS

}  // namespace fl
