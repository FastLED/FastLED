#pragma once

// Generic STM32F1 (BluePill F103C8 and similar) Pin Definitions
// Included by families/stm32f1.h - do not include directly
// Defines pin mappings inside fl:: namespace
//
// This is a generic F1 mapping using hardware pin names (PA0-PA15, PB0-PB15, etc.)
// rather than Arduino pin numbers, as F1 boards don't always use STM32duino's
// pin numbering scheme.

#define MAX_PIN PB1

// Port B pins
_DEFPIN_ARM_F1(PB11, 11, B);
_DEFPIN_ARM_F1(PB10, 10, B);
_DEFPIN_ARM_F1(PB2, 2, B);
_DEFPIN_ARM_F1(PB0, 0, B);
_DEFPIN_ARM_F1(PB7, 7, B);
_DEFPIN_ARM_F1(PB6, 6, B);
_DEFPIN_ARM_F1(PB5, 5, B);
_DEFPIN_ARM_F1(PB4, 4, B);
_DEFPIN_ARM_F1(PB3, 3, B);
_DEFPIN_ARM_F1(PB15, 15, B);
_DEFPIN_ARM_F1(PB14, 14, B);
_DEFPIN_ARM_F1(PB13, 13, B);
_DEFPIN_ARM_F1(PB12, 12, B);
_DEFPIN_ARM_F1(PB8, 8, B);
_DEFPIN_ARM_F1(PB1, 1, B);

// Port A pins
_DEFPIN_ARM_F1(PA7, 7, A);
_DEFPIN_ARM_F1(PA6, 6, A);
_DEFPIN_ARM_F1(PA5, 5, A);
_DEFPIN_ARM_F1(PA4, 4, A);
_DEFPIN_ARM_F1(PA3, 3, A);
_DEFPIN_ARM_F1(PA2, 2, A);
_DEFPIN_ARM_F1(PA1, 1, A);
_DEFPIN_ARM_F1(PA0, 0, A);
_DEFPIN_ARM_F1(PA15, 15, A);
_DEFPIN_ARM_F1(PA14, 14, A);
_DEFPIN_ARM_F1(PA13, 13, A);
_DEFPIN_ARM_F1(PA12, 12, A);
_DEFPIN_ARM_F1(PA11, 11, A);
_DEFPIN_ARM_F1(PA10, 10, A);
_DEFPIN_ARM_F1(PA9, 9, A);
_DEFPIN_ARM_F1(PA8, 8, A);

// Port C pins
_DEFPIN_ARM_F1(PC15, 15, C);
_DEFPIN_ARM_F1(PC14, 14, C);
_DEFPIN_ARM_F1(PC13, 13, C);

// SPI2 pins (BluePill default)
#define SPI_DATA PB15
#define SPI_CLOCK PB13
