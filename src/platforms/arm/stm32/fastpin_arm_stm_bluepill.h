#pragma once

#include "namespace.h"

FASTLED_NAMESPACE_BEGIN

#if defined(FASTLED_FORCE_SOFTWARE_PINS)
#warning "Software pin support forced, pin access will be slightly slower."
#define NO_HARDWARE_PIN_SUPPORT
#undef HAS_HARDWARE_PIN_SUPPORT
#else
#include "armpin.h"

#define _RD32(T) struct __gen_struct_ ## T { static FASTLED_FORCE_INLINE gpio_reg_map* r() { return T->regs; } };
#define _FL_IO(L,C) _RD32(GPIO ## L);  _FL_DEFINE_PORT3(L, C, _R(GPIO ## L));

#ifdef GPIOA
_FL_IO(A,0);
#endif
#ifdef GPIOB
_FL_IO(B,1);
#endif
#ifdef GPIOC
_FL_IO(C,2);
#endif
#ifdef GPIOD
_FL_IO(D,3);
#endif
#ifdef GPIOE
_FL_IO(E,4);
#endif
#ifdef GPIOF
_FL_IO(F,5);
#endif
#ifdef GPIOG
_FL_IO(G,6);
#endif

#define MAX_PIN PB1

_FL_DEFPIN(PB11, 11, B);
_FL_DEFPIN(PB10, 10, B);
_FL_DEFPIN(PB2, 2, B);
_FL_DEFPIN(PB0, 0, B);
_FL_DEFPIN(PA7, 7, A);
_FL_DEFPIN(PA6, 6, A);
_FL_DEFPIN(PA5, 5, A);
_FL_DEFPIN(PA4, 4, A);
_FL_DEFPIN(PA3, 3, A);
_FL_DEFPIN(PA2, 2, A);
_FL_DEFPIN(PA1, 1, A);
_FL_DEFPIN(PA0, 0, A);
_FL_DEFPIN(PC15, 15, C);
_FL_DEFPIN(PC14, 14, C);
_FL_DEFPIN(PC13, 13, C);
_FL_DEFPIN(PB7, 7, B);
_FL_DEFPIN(PB6, 6, B);
_FL_DEFPIN(PB5, 5, B);
_FL_DEFPIN(PB4, 4, B);
_FL_DEFPIN(PB3, 3, B);
_FL_DEFPIN(PA15, 15, A);
_FL_DEFPIN(PA14, 14, A);
_FL_DEFPIN(PA13, 13, A);
_FL_DEFPIN(PA12, 12, A);
_FL_DEFPIN(PA11, 11, A);
_FL_DEFPIN(PA10, 10, A);
_FL_DEFPIN(PA9, 9, A);
_FL_DEFPIN(PA8, 8, A);
_FL_DEFPIN(PB15, 15, B);
_FL_DEFPIN(PB14, 14, B);
_FL_DEFPIN(PB13, 13, B);
_FL_DEFPIN(PB12, 12, B);
_FL_DEFPIN(PB8, 8, B);
_FL_DEFPIN(PB1, 1, B);



#define SPI_DATA BOARD_SPI1_MOSI_PIN
#define SPI_CLOCK BOARD_SPI1_SCK_PIN

#define HAS_HARDWARE_PIN_SUPPORT

#endif  // FASTLED_FORCE_SOFTWARE_PINS

FASTLED_NAMESPACE_END
