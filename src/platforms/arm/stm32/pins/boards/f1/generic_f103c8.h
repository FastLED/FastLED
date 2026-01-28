#pragma once
// ok no namespace fl

// Generic STM32F103C8 (STM32duino) Pin Definitions
// Included by families/stm32f1.h - do not include directly
// Defines pin mappings inside fl:: namespace
//
// Arduino-style pin numbering for STM32duino

// IWYU pragma: private, include "platforms/arm/stm32/pins/families/stm32f1.h"

#include "platforms/arm/stm32/pins/core/armpin_template.h"
#include "platforms/arm/stm32/pins/core/gpio_port_init.h"
#include "platforms/arm/stm32/pins/core/pin_macros.h"
#include "platforms/arm/stm32/pins/families/stm32f1.h"

#define MAX_PIN 36

// PA0-PA15
_DEFPIN_ARM_F1(0, 0, A);
_DEFPIN_ARM_F1(1, 1, A);
_DEFPIN_ARM_F1(2, 2, A);
_DEFPIN_ARM_F1(3, 3, A);
_DEFPIN_ARM_F1(4, 4, A);
_DEFPIN_ARM_F1(5, 5, A);
_DEFPIN_ARM_F1(6, 6, A);
_DEFPIN_ARM_F1(7, 7, A);
_DEFPIN_ARM_F1(8, 8, A);
_DEFPIN_ARM_F1(9, 9, A);
_DEFPIN_ARM_F1(10, 10, A);
_DEFPIN_ARM_F1(11, 11, A);
_DEFPIN_ARM_F1(12, 12, A);
_DEFPIN_ARM_F1(13, 13, A);
_DEFPIN_ARM_F1(14, 14, A);
_DEFPIN_ARM_F1(15, 15, A);

// PB0-PB15
_DEFPIN_ARM_F1(16, 0, B);
_DEFPIN_ARM_F1(17, 1, B);
_DEFPIN_ARM_F1(18, 2, B);
_DEFPIN_ARM_F1(19, 3, B);
_DEFPIN_ARM_F1(20, 4, B);
_DEFPIN_ARM_F1(21, 5, B);
_DEFPIN_ARM_F1(22, 6, B);
_DEFPIN_ARM_F1(23, 7, B);
_DEFPIN_ARM_F1(24, 8, B);
_DEFPIN_ARM_F1(25, 9, B);
_DEFPIN_ARM_F1(26, 10, B);
_DEFPIN_ARM_F1(27, 11, B);
_DEFPIN_ARM_F1(28, 12, B);
_DEFPIN_ARM_F1(29, 13, B);
_DEFPIN_ARM_F1(30, 14, B);
_DEFPIN_ARM_F1(31, 15, B);

// PC13-PC15
_DEFPIN_ARM_F1(32, 13, C);
_DEFPIN_ARM_F1(33, 14, C);
_DEFPIN_ARM_F1(34, 15, C);

// PD0-PD1
_DEFPIN_ARM_F1(35, 0, D);
_DEFPIN_ARM_F1(36, 1, D);

// SPI2 MOSI
#define SPI_DATA PB15
// SPI2 SCK
#define SPI_CLOCK PB13
