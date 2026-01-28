#pragma once
// ok no namespace fl

// Discovery F407VG Pin Definitions
// Included by families/stm32f4.h - do not include directly
// Defines pin mappings inside fl:: namespace
// STM32F407VGT6 - 79 pins

// IWYU pragma: private, include "platforms/arm/stm32/pins/families/stm32f4.h"

#include "platforms/arm/stm32/pins/core/armpin_template.h"
#include "platforms/arm/stm32/pins/core/gpio_port_init.h"
#include "platforms/arm/stm32/pins/core/pin_macros.h"
#include "platforms/arm/stm32/pins/families/stm32f4.h"

// Port C
_DEFPIN_ARM_F4(0, 0, C);   // PC_0
_DEFPIN_ARM_F4(1, 2, C);   // PC_2
// Port A
_DEFPIN_ARM_F4(2, 0, A);   // PA_0
_DEFPIN_ARM_F4(3, 2, A);   // PA_2
_DEFPIN_ARM_F4(4, 4, A);   // PA_4
_DEFPIN_ARM_F4(5, 6, A);   // PA_6
// Port C
_DEFPIN_ARM_F4(6, 4, C);   // PC_4
// Port B
_DEFPIN_ARM_F4(7, 0, B);   // PB_0
_DEFPIN_ARM_F4(8, 2, B);   // PB_2
// Port E
_DEFPIN_ARM_F4(9, 8, E);   // PE_8
_DEFPIN_ARM_F4(10, 10, E); // PE_10
_DEFPIN_ARM_F4(11, 12, E); // PE_12
_DEFPIN_ARM_F4(12, 14, E); // PE_14
// Port B
_DEFPIN_ARM_F4(13, 10, B); // PB_10
_DEFPIN_ARM_F4(14, 12, B); // PB_12
_DEFPIN_ARM_F4(15, 14, B); // PB_14
// Port D
_DEFPIN_ARM_F4(16, 8, D);  // PD_8
_DEFPIN_ARM_F4(17, 10, D); // PD_10
_DEFPIN_ARM_F4(18, 12, D); // PD_12
_DEFPIN_ARM_F4(19, 14, D); // PD_14
// Port H
_DEFPIN_ARM_F4(20, 0, H);  // PH_0
// Port C
_DEFPIN_ARM_F4(21, 14, C); // PC_14
// Port E
_DEFPIN_ARM_F4(22, 6, E);  // PE_6
_DEFPIN_ARM_F4(23, 4, E);  // PE_4
_DEFPIN_ARM_F4(24, 2, E);  // PE_2
_DEFPIN_ARM_F4(25, 0, E);  // PE_0
// Port B
_DEFPIN_ARM_F4(26, 8, B);  // PB_8
_DEFPIN_ARM_F4(27, 6, B);  // PB_6
_DEFPIN_ARM_F4(28, 4, B);  // PB_4
// Port D
_DEFPIN_ARM_F4(29, 7, D);  // PD_7
_DEFPIN_ARM_F4(30, 5, D);  // PD_5
_DEFPIN_ARM_F4(31, 3, D);  // PD_3
_DEFPIN_ARM_F4(32, 1, D);  // PD_1
// Port C
_DEFPIN_ARM_F4(33, 12, C); // PC_12
_DEFPIN_ARM_F4(34, 10, C); // PC_10
// Port A
_DEFPIN_ARM_F4(35, 10, A); // PA_10
_DEFPIN_ARM_F4(36, 8, A);  // PA_8
// Port C
_DEFPIN_ARM_F4(37, 8, C);  // PC_8
_DEFPIN_ARM_F4(38, 6, C);  // PC_6
_DEFPIN_ARM_F4(39, 1, C);  // PC_1
_DEFPIN_ARM_F4(40, 3, C);  // PC_3
// Port A
_DEFPIN_ARM_F4(41, 1, A);  // PA_1
_DEFPIN_ARM_F4(42, 3, A);  // PA_3
_DEFPIN_ARM_F4(43, 5, A);  // PA_5
_DEFPIN_ARM_F4(44, 7, A);  // PA_7
// Port C
_DEFPIN_ARM_F4(45, 5, C);  // PC_5
// Port B
_DEFPIN_ARM_F4(46, 1, B);  // PB_1
// Port E
_DEFPIN_ARM_F4(47, 7, E);  // PE_7
_DEFPIN_ARM_F4(48, 9, E);  // PE_9
_DEFPIN_ARM_F4(49, 11, E); // PE_11
_DEFPIN_ARM_F4(50, 13, E); // PE_13
_DEFPIN_ARM_F4(51, 15, E); // PE_15
// Port B
_DEFPIN_ARM_F4(52, 11, B); // PB_11
_DEFPIN_ARM_F4(53, 13, B); // PB_13
_DEFPIN_ARM_F4(54, 15, B); // PB_15
// Port D
_DEFPIN_ARM_F4(55, 9, D);  // PD_9
_DEFPIN_ARM_F4(56, 11, D); // PD_11
_DEFPIN_ARM_F4(57, 13, D); // PD_13
_DEFPIN_ARM_F4(58, 15, D); // PD_15
// Port H
_DEFPIN_ARM_F4(59, 1, H);  // PH_1
// Port C
_DEFPIN_ARM_F4(60, 15, C); // PC_15
_DEFPIN_ARM_F4(61, 13, C); // PC_13
// Port E
_DEFPIN_ARM_F4(62, 5, E);  // PE_5
_DEFPIN_ARM_F4(63, 3, E);  // PE_3
_DEFPIN_ARM_F4(64, 1, E);  // PE_1
// Port B
_DEFPIN_ARM_F4(65, 9, B);  // PB_9
_DEFPIN_ARM_F4(66, 7, B);  // PB_7
_DEFPIN_ARM_F4(67, 5, B);  // PB_5
_DEFPIN_ARM_F4(68, 3, B);  // PB_3
// Port D
_DEFPIN_ARM_F4(69, 6, D);  // PD_6
_DEFPIN_ARM_F4(70, 4, D);  // PD_4
_DEFPIN_ARM_F4(71, 2, D);  // PD_2
_DEFPIN_ARM_F4(72, 0, D);  // PD_0
// Port C
_DEFPIN_ARM_F4(73, 11, C); // PC_11
// Port A
_DEFPIN_ARM_F4(74, 15, A); // PA_15
_DEFPIN_ARM_F4(75, 13, A); // PA_13
_DEFPIN_ARM_F4(76, 9, A);  // PA_9
// Port C
_DEFPIN_ARM_F4(77, 9, C);  // PC_9
_DEFPIN_ARM_F4(78, 7, C);  // PC_7
