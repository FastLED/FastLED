#pragma once
// ok no namespace fl

// BlackPill F401Cx Pin Definitions
// Included by families/stm32f4.h - do not include directly
// Defines pin mappings inside fl:: namespace (F401CC / F401CE)
// STM32F401CCU6 / STM32F401CEU6 - 36 pins

// IWYU pragma: private, include "platforms/arm/stm32/pins/families/stm32f4.h"

#include "platforms/arm/stm32/pins/core/armpin_template.h"
#include "platforms/arm/stm32/pins/core/gpio_port_init.h"
#include "platforms/arm/stm32/pins/core/pin_macros.h"
#include "platforms/arm/stm32/pins/families/stm32f4.h"

// Port A
_DEFPIN_ARM_F4(0, 0, A);   // PA_0
_DEFPIN_ARM_F4(1, 1, A);   // PA_1
_DEFPIN_ARM_F4(2, 2, A);   // PA_2
_DEFPIN_ARM_F4(3, 3, A);   // PA_3
_DEFPIN_ARM_F4(4, 4, A);   // PA_4
_DEFPIN_ARM_F4(5, 5, A);   // PA_5
_DEFPIN_ARM_F4(6, 6, A);   // PA_6
_DEFPIN_ARM_F4(7, 7, A);   // PA_7
_DEFPIN_ARM_F4(8, 8, A);   // PA_8
_DEFPIN_ARM_F4(9, 9, A);   // PA_9
_DEFPIN_ARM_F4(10, 10, A); // PA_10
_DEFPIN_ARM_F4(11, 11, A); // PA_11
_DEFPIN_ARM_F4(12, 12, A); // PA_12
_DEFPIN_ARM_F4(13, 13, A); // PA_13
_DEFPIN_ARM_F4(14, 14, A); // PA_14
_DEFPIN_ARM_F4(15, 15, A); // PA_15
// Port B
_DEFPIN_ARM_F4(16, 0, B);  // PB_0
_DEFPIN_ARM_F4(17, 1, B);  // PB_1
_DEFPIN_ARM_F4(18, 2, B);  // PB_2
_DEFPIN_ARM_F4(19, 3, B);  // PB_3
_DEFPIN_ARM_F4(20, 4, B);  // PB_4
_DEFPIN_ARM_F4(21, 5, B);  // PB_5
_DEFPIN_ARM_F4(22, 6, B);  // PB_6
_DEFPIN_ARM_F4(23, 7, B);  // PB_7
_DEFPIN_ARM_F4(24, 8, B);  // PB_8
_DEFPIN_ARM_F4(25, 9, B);  // PB_9
_DEFPIN_ARM_F4(26, 10, B); // PB_10
_DEFPIN_ARM_F4(27, 12, B); // PB_12
_DEFPIN_ARM_F4(28, 13, B); // PB_13
_DEFPIN_ARM_F4(29, 14, B); // PB_14
_DEFPIN_ARM_F4(30, 15, B); // PB_15
// Port C
_DEFPIN_ARM_F4(31, 13, C); // PC_13
_DEFPIN_ARM_F4(32, 14, C); // PC_14
_DEFPIN_ARM_F4(33, 15, C); // PC_15
// Port H
_DEFPIN_ARM_F4(34, 0, H);  // PH_0
_DEFPIN_ARM_F4(35, 1, H);  // PH_1
