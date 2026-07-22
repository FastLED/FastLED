#pragma once
// ok no namespace fl

// Generic STM32G0 Family Pin Definitions (all STM32G0 devices: G030, G031, G050, G070, G071, G081, G0B0, G0B1, G0C1, etc.)
// Included by families/stm32g0.h - do not include directly
// Defines pin mappings inside fl:: namespace
// Maps pin numbers directly according to standard STM32duino PinName definitions:
// PA0-PA15 = 0-15  (PA4 = pin 4)
// PB0-PB15 = 16-31 (PB5 = pin 21)
// PC0-PC15 = 32-47
// PD0-PD15 = 48-63
// PF0-PF15 = 64-79

// IWYU pragma: private, include "platforms/arm/stm32/pins/families/stm32g0.h"

#include "platforms/arm/stm32/pins/core/armpin_template.h"
#include "platforms/arm/stm32/pins/core/gpio_port_init.h"
#include "platforms/arm/stm32/pins/core/pin_macros.h"
#include "platforms/arm/stm32/pins/families/stm32g0.h"

// PA0-PA15 (pins 0-15)
_DEFPIN_ARM_G0(0, 0, A);
_DEFPIN_ARM_G0(1, 1, A);
_DEFPIN_ARM_G0(2, 2, A);
_DEFPIN_ARM_G0(3, 3, A);
_DEFPIN_ARM_G0(4, 4, A);
_DEFPIN_ARM_G0(5, 5, A);
_DEFPIN_ARM_G0(6, 6, A);
_DEFPIN_ARM_G0(7, 7, A);
_DEFPIN_ARM_G0(8, 8, A);
_DEFPIN_ARM_G0(9, 9, A);
_DEFPIN_ARM_G0(10, 10, A);
_DEFPIN_ARM_G0(11, 11, A);
_DEFPIN_ARM_G0(12, 12, A);
_DEFPIN_ARM_G0(13, 13, A);
_DEFPIN_ARM_G0(14, 14, A);
_DEFPIN_ARM_G0(15, 15, A);

// PB0-PB15 (pins 16-31)
_DEFPIN_ARM_G0(16, 0, B);
_DEFPIN_ARM_G0(17, 1, B);
_DEFPIN_ARM_G0(18, 2, B);
_DEFPIN_ARM_G0(19, 3, B);
_DEFPIN_ARM_G0(20, 4, B);
_DEFPIN_ARM_G0(21, 5, B);
_DEFPIN_ARM_G0(22, 6, B);
_DEFPIN_ARM_G0(23, 7, B);
_DEFPIN_ARM_G0(24, 8, B);
_DEFPIN_ARM_G0(25, 9, B);
_DEFPIN_ARM_G0(26, 10, B);
_DEFPIN_ARM_G0(27, 11, B);
_DEFPIN_ARM_G0(28, 12, B);
_DEFPIN_ARM_G0(29, 13, B);
_DEFPIN_ARM_G0(30, 14, B);
_DEFPIN_ARM_G0(31, 15, B);

// PC0-PC15 (pins 32-47)
_DEFPIN_ARM_G0(32, 0, C);
_DEFPIN_ARM_G0(33, 1, C);
_DEFPIN_ARM_G0(34, 2, C);
_DEFPIN_ARM_G0(35, 3, C);
_DEFPIN_ARM_G0(36, 4, C);
_DEFPIN_ARM_G0(37, 5, C);
_DEFPIN_ARM_G0(38, 6, C);
_DEFPIN_ARM_G0(39, 7, C);
_DEFPIN_ARM_G0(40, 8, C);
_DEFPIN_ARM_G0(41, 9, C);
_DEFPIN_ARM_G0(42, 10, C);
_DEFPIN_ARM_G0(43, 11, C);
_DEFPIN_ARM_G0(44, 12, C);
_DEFPIN_ARM_G0(45, 13, C);
_DEFPIN_ARM_G0(46, 14, C);
_DEFPIN_ARM_G0(47, 15, C);

// PD0-PD15 (pins 48-63)
_DEFPIN_ARM_G0(48, 0, D);
_DEFPIN_ARM_G0(49, 1, D);
_DEFPIN_ARM_G0(50, 2, D);
_DEFPIN_ARM_G0(51, 3, D);
_DEFPIN_ARM_G0(52, 4, D);
_DEFPIN_ARM_G0(53, 5, D);
_DEFPIN_ARM_G0(54, 6, D);
_DEFPIN_ARM_G0(55, 7, D);
_DEFPIN_ARM_G0(56, 8, D);
_DEFPIN_ARM_G0(57, 9, D);
_DEFPIN_ARM_G0(58, 10, D);
_DEFPIN_ARM_G0(59, 11, D);
_DEFPIN_ARM_G0(60, 12, D);
_DEFPIN_ARM_G0(61, 13, D);
_DEFPIN_ARM_G0(62, 14, D);
_DEFPIN_ARM_G0(63, 15, D);

// PF0-PF15 (pins 64-79)
_DEFPIN_ARM_G0(64, 0, F);
_DEFPIN_ARM_G0(65, 1, F);
_DEFPIN_ARM_G0(66, 2, F);
_DEFPIN_ARM_G0(67, 3, F);
_DEFPIN_ARM_G0(68, 4, F);
_DEFPIN_ARM_G0(69, 5, F);
_DEFPIN_ARM_G0(70, 6, F);
_DEFPIN_ARM_G0(71, 7, F);
_DEFPIN_ARM_G0(72, 8, F);
_DEFPIN_ARM_G0(73, 9, F);
_DEFPIN_ARM_G0(74, 10, F);
_DEFPIN_ARM_G0(75, 11, F);
_DEFPIN_ARM_G0(76, 12, F);
_DEFPIN_ARM_G0(77, 13, F);
_DEFPIN_ARM_G0(78, 14, F);
_DEFPIN_ARM_G0(79, 15, F);
