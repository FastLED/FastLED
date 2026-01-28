#pragma once
// ok no namespace fl

// Spark Core (Sparkfun STM32F103-based board) Pin Definitions
// Included by families/stm32f2.h - do not include directly
// Defines pin mappings inside fl:: namespace
//
// Note: Despite being F103-based, Spark Core uses F2-style register access

// IWYU pragma: private, include "platforms/arm/stm32/pins/families/stm32f2.h"

#include "platforms/arm/stm32/pins/core/armpin_template.h"
#include "platforms/arm/stm32/pins/core/gpio_port_init.h"
#include "platforms/arm/stm32/pins/core/pin_macros.h"
#include "platforms/arm/stm32/pins/families/stm32f2.h"

#define MAX_PIN 19

_DEFPIN_ARM_F2(0, 7, B);
_DEFPIN_ARM_F2(1, 6, B);
_DEFPIN_ARM_F2(2, 5, B);
_DEFPIN_ARM_F2(3, 4, B);
_DEFPIN_ARM_F2(4, 3, B);
_DEFPIN_ARM_F2(5, 15, A);
_DEFPIN_ARM_F2(6, 14, A);
_DEFPIN_ARM_F2(7, 13, A);
_DEFPIN_ARM_F2(8, 8, A);
_DEFPIN_ARM_F2(9, 9, A);
_DEFPIN_ARM_F2(10, 0, A);
_DEFPIN_ARM_F2(11, 1, A);
_DEFPIN_ARM_F2(12, 4, A);
_DEFPIN_ARM_F2(13, 5, A);
_DEFPIN_ARM_F2(14, 6, A);
_DEFPIN_ARM_F2(15, 7, A);
_DEFPIN_ARM_F2(16, 0, B);
_DEFPIN_ARM_F2(17, 1, B);
_DEFPIN_ARM_F2(18, 3, A);
_DEFPIN_ARM_F2(19, 2, A);

#define SPI_DATA 15
#define SPI_CLOCK 13
