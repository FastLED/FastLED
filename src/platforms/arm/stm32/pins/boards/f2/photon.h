#pragma once
// ok no namespace fl

// Particle Photon (STM32F205) Pin Definitions
// Included by families/stm32f2.h - do not include directly
// Defines pin mappings inside fl:: namespace
//
// Reference: https://github.com/focalintent/FastLED-Sparkcore/blob/master/firmware/fastpin_arm_stm32.h

#define MAX_PIN 20

_DEFPIN_ARM_F2(0, 7, B);
_DEFPIN_ARM_F2(1, 6, B);
_DEFPIN_ARM_F2(2, 5, B);
_DEFPIN_ARM_F2(3, 4, B);
_DEFPIN_ARM_F2(4, 3, B);
_DEFPIN_ARM_F2(5, 15, A);
_DEFPIN_ARM_F2(6, 14, A);
_DEFPIN_ARM_F2(7, 13, A);
_DEFPIN_ARM_F2(10, 5, C);
_DEFPIN_ARM_F2(11, 3, C);
_DEFPIN_ARM_F2(12, 2, C);
_DEFPIN_ARM_F2(13, 5, A);
_DEFPIN_ARM_F2(14, 6, A);
_DEFPIN_ARM_F2(15, 7, A);
_DEFPIN_ARM_F2(16, 4, A);
_DEFPIN_ARM_F2(17, 0, A);
_DEFPIN_ARM_F2(18, 10, A);
_DEFPIN_ARM_F2(19, 9, A);
_DEFPIN_ARM_F2(20, 7, C);

#define SPI_DATA 15
#define SPI_CLOCK 13
