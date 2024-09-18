#pragma once

#include "namespace.h"

// Grabbed from https://github.com/13rac1/FastLED-STM32/blob/stm32f103/platforms/arm/stm32/fastpin_arm_stm32.h
// It looks like this might work for all STM32F1 boards and more.

#include "namespace.h"
#include "force_inline.h"

FASTLED_NAMESPACE_BEGIN

#include "armpin.h"

// Actual pin definitions
#if defined(SPARK)
#define _RD32(T) struct __gen_struct_ ## T { static __attribute__((always_inline)) inline volatile GPIO_TypeDef * r() { return T; } };
#define _IO32(L) _RD32(GPIO ## L)


_IO32(A); _IO32(B); _IO32(C); _IO32(D); _IO32(E); _IO32(F); _IO32(G);


#define MAX_PIN 19
_DEFPIN_ARM(0, 7, B);
_DEFPIN_ARM(1, 6, B);
_DEFPIN_ARM(2, 5, B);
_DEFPIN_ARM(3, 4, B);
_DEFPIN_ARM(4, 3, B);
_DEFPIN_ARM(5, 15, A);
_DEFPIN_ARM(6, 14, A);
_DEFPIN_ARM(7, 13, A);
_DEFPIN_ARM(8, 8, A);
_DEFPIN_ARM(9, 9, A);
_DEFPIN_ARM(10, 0, A);
_DEFPIN_ARM(11, 1, A);
_DEFPIN_ARM(12, 4, A);
_DEFPIN_ARM(13, 5, A);
_DEFPIN_ARM(14, 6, A);
_DEFPIN_ARM(15, 7, A);
_DEFPIN_ARM(16, 0, B);
_DEFPIN_ARM(17, 1, B);
_DEFPIN_ARM(18, 3, A);
_DEFPIN_ARM(19, 2, A);


#define SPI_DATA 15
#define SPI_CLOCK 13

#define HAS_HARDWARE_PIN_SUPPORT

#elif defined(__STM32F1__)  // bluepill


#define _RD32(T) struct __gen_struct_ ## T { static __attribute__((always_inline)) inline gpio_reg_map* r() { return T->regs; } };
#define _IO32(L) _RD32(GPIO ## L)

_IO32(A); _IO32(B); _IO32(C); _IO32(D);

#define MAX_PIN PB1

_DEFPIN_ARM(PB11, 11, B);
_DEFPIN_ARM(PB10, 10, B);
_DEFPIN_ARM(PB2, 2, B);
_DEFPIN_ARM(PB0, 0, B);
_DEFPIN_ARM(PA7, 7, A);
_DEFPIN_ARM(PA6, 6, A);
_DEFPIN_ARM(PA5, 5, A);
_DEFPIN_ARM(PA4, 4, A);
_DEFPIN_ARM(PA3, 3, A);
_DEFPIN_ARM(PA2, 2, A);
_DEFPIN_ARM(PA1, 1, A);
_DEFPIN_ARM(PA0, 0, A);
_DEFPIN_ARM(PC15, 15, C);
_DEFPIN_ARM(PC14, 14, C);
_DEFPIN_ARM(PC13, 13, C);
_DEFPIN_ARM(PB7, 7, B);
_DEFPIN_ARM(PB6, 6, B);
_DEFPIN_ARM(PB5, 5, B);
_DEFPIN_ARM(PB4, 4, B);
_DEFPIN_ARM(PB3, 3, B);
_DEFPIN_ARM(PA15, 15, A);
_DEFPIN_ARM(PA14, 14, A);
_DEFPIN_ARM(PA13, 13, A);
_DEFPIN_ARM(PA12, 12, A);
_DEFPIN_ARM(PA11, 11, A);
_DEFPIN_ARM(PA10, 10, A);
_DEFPIN_ARM(PA9, 9, A);
_DEFPIN_ARM(PA8, 8, A);
_DEFPIN_ARM(PB15, 15, B);
_DEFPIN_ARM(PB14, 14, B);
_DEFPIN_ARM(PB13, 13, B);
_DEFPIN_ARM(PB12, 12, B);
_DEFPIN_ARM(PB8, 8, B);
_DEFPIN_ARM(PB1, 1, B);

#define SPI_DATA BOARD_SPI1_MOSI_PIN
#define SPI_CLOCK BOARD_SPI1_SCK_PIN 

#define HAS_HARDWARE_PIN_SUPPORT

#else
#error "Please define pin mappings for your board"

#endif

FASTLED_NAMESPACE_END

