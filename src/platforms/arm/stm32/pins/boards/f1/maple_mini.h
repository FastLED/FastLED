#pragma once
// ok no namespace fl

// Maple Mini (STM32F103CB) Pin Definitions
// Included by families/stm32f1.h - do not include directly
// Defines pin mappings inside fl:: namespace
//
// Uses libmaple pin numbering (not STM32duino)

// IWYU pragma: private, include "platforms/arm/stm32/pins/families/stm32f1.h"

#include "platforms/arm/stm32/pins/core/armpin_template.h"
#include "platforms/arm/stm32/pins/core/gpio_port_init.h"
#include "platforms/arm/stm32/pins/core/pin_macros.h"
#include "platforms/arm/stm32/pins/families/stm32f1.h"

#define MAX_PIN 46

_DEFPIN_ARM_F1(10, 0, A);  // PA0 - PA7
_DEFPIN_ARM_F1(11, 1, A);
_DEFPIN_ARM_F1(12, 2, A);
_DEFPIN_ARM_F1(13, 3, A);
_DEFPIN_ARM_F1(14, 4, A);
_DEFPIN_ARM_F1(15, 5, A);
_DEFPIN_ARM_F1(16, 6, A);
_DEFPIN_ARM_F1(17, 7, A);
_DEFPIN_ARM_F1(29, 8, A);  // PA8 - PA15
_DEFPIN_ARM_F1(30, 9, A);
_DEFPIN_ARM_F1(31, 10, A);
_DEFPIN_ARM_F1(32, 11, A);
_DEFPIN_ARM_F1(33, 12, A);
_DEFPIN_ARM_F1(34, 13, A);
_DEFPIN_ARM_F1(37, 14, A);
_DEFPIN_ARM_F1(38, 15, A);

_DEFPIN_ARM_F1(18, 0, B);  // PB0 - PB11
_DEFPIN_ARM_F1(19, 1, B);
_DEFPIN_ARM_F1(20, 2, B);
_DEFPIN_ARM_F1(39, 3, B);
_DEFPIN_ARM_F1(40, 4, B);
_DEFPIN_ARM_F1(41, 5, B);
_DEFPIN_ARM_F1(42, 6, B);
_DEFPIN_ARM_F1(43, 7, B);
_DEFPIN_ARM_F1(45, 8, B);
_DEFPIN_ARM_F1(46, 9, B);
_DEFPIN_ARM_F1(21, 10, B);
_DEFPIN_ARM_F1(22, 11, B);

_DEFPIN_ARM_F1(2, 13, C);  // PC13 - PC15
_DEFPIN_ARM_F1(3, 14, C);
_DEFPIN_ARM_F1(4, 15, C);

#define SPI_DATA BOARD_SPI1_MOSI_PIN
#define SPI_CLOCK BOARD_SPI1_SCK_PIN
