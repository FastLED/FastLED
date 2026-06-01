#pragma once
// ok no namespace fl

// Arduino UNO Q STM32U585 pin definitions.
// Included by families/stm32u5.h - do not include directly.

// IWYU pragma: private, include "platforms/arm/stm32/pins/families/stm32u5.h"

#ifndef _DEFPIN_ARM_U5
  #error "This file must be included via families/stm32u5.h which provides the _DEFPIN_ARM_U5 macro"
#endif

// Arduino R3 header
_DEFPIN_ARM_U5(0, 7, B);    // D0 - PB7
_DEFPIN_ARM_U5(1, 6, B);    // D1 - PB6
_DEFPIN_ARM_U5(2, 3, B);    // D2 - PB3
_DEFPIN_ARM_U5(3, 0, B);    // D3 - PB0
_DEFPIN_ARM_U5(4, 12, A);   // D4 - PA12
_DEFPIN_ARM_U5(5, 11, A);   // D5 - PA11
_DEFPIN_ARM_U5(6, 1, B);    // D6 - PB1
_DEFPIN_ARM_U5(7, 2, B);    // D7 - PB2
_DEFPIN_ARM_U5(8, 4, B);    // D8 - PB4
_DEFPIN_ARM_U5(9, 8, B);    // D9 - PB8
_DEFPIN_ARM_U5(10, 9, B);   // D10 - PB9
_DEFPIN_ARM_U5(11, 15, B);  // D11 - PB15
_DEFPIN_ARM_U5(12, 14, B);  // D12 - PB14
_DEFPIN_ARM_U5(13, 13, B);  // D13 - PB13

// Analog header, exposed as digital pins 14-19.
_DEFPIN_ARM_U5(14, 4, A);   // A0 - PA4
_DEFPIN_ARM_U5(15, 5, A);   // A1 - PA5
_DEFPIN_ARM_U5(16, 6, A);   // A2 - PA6
_DEFPIN_ARM_U5(17, 7, A);   // A3 - PA7
_DEFPIN_ARM_U5(18, 1, C);   // A4 - PC1
_DEFPIN_ARM_U5(19, 0, C);   // A5 - PC0

_DEFPIN_ARM_U5(20, 11, B);  // D20 - PB11
_DEFPIN_ARM_U5(21, 10, B);  // D21 - PB10

// JSPI/JMISC and internal expansion pins from ArduinoCore-zephyr's UNO Q variant.
_DEFPIN_ARM_U5(22, 2, C);
_DEFPIN_ARM_U5(23, 3, C);
_DEFPIN_ARM_U5(24, 1, D);
_DEFPIN_ARM_U5(25, 6, C);
_DEFPIN_ARM_U5(26, 2, D);
_DEFPIN_ARM_U5(27, 7, C);
_DEFPIN_ARM_U5(28, 2, E);
_DEFPIN_ARM_U5(29, 8, C);
_DEFPIN_ARM_U5(30, 3, E);
_DEFPIN_ARM_U5(31, 9, C);
_DEFPIN_ARM_U5(32, 5, E);
_DEFPIN_ARM_U5(33, 4, E);
_DEFPIN_ARM_U5(34, 6, E);
_DEFPIN_ARM_U5(35, 4, I);
_DEFPIN_ARM_U5(36, 7, E);
_DEFPIN_ARM_U5(37, 6, I);
_DEFPIN_ARM_U5(38, 8, E);
_DEFPIN_ARM_U5(39, 7, I);
_DEFPIN_ARM_U5(40, 14, F);
_DEFPIN_ARM_U5(41, 9, D);
_DEFPIN_ARM_U5(42, 15, F);
_DEFPIN_ARM_U5(43, 5, I);
_DEFPIN_ARM_U5(44, 3, A);
_DEFPIN_ARM_U5(45, 8, D);
_DEFPIN_ARM_U5(46, 0, A);
_DEFPIN_ARM_U5(47, 8, A);
_DEFPIN_ARM_U5(48, 1, A);
_DEFPIN_ARM_U5(49, 10, A);

// On-board LEDs.
_DEFPIN_ARM_U5(50, 10, H);
_DEFPIN_ARM_U5(51, 11, H);
_DEFPIN_ARM_U5(52, 12, H);
_DEFPIN_ARM_U5(53, 13, H);
_DEFPIN_ARM_U5(54, 14, H);
_DEFPIN_ARM_U5(55, 15, H);

// LED matrix and control pins.
_DEFPIN_ARM_U5(56, 0, F);
_DEFPIN_ARM_U5(57, 1, F);
_DEFPIN_ARM_U5(58, 2, F);
_DEFPIN_ARM_U5(59, 3, F);
_DEFPIN_ARM_U5(60, 4, F);
_DEFPIN_ARM_U5(61, 5, F);
_DEFPIN_ARM_U5(62, 6, F);
_DEFPIN_ARM_U5(63, 7, F);
_DEFPIN_ARM_U5(64, 8, F);
_DEFPIN_ARM_U5(65, 9, F);
_DEFPIN_ARM_U5(66, 10, F);
_DEFPIN_ARM_U5(67, 13, G);
_DEFPIN_ARM_U5(68, 2, A);
_DEFPIN_ARM_U5(69, 3, H);
