#pragma once
// ok no namespace fl

// Nucleo F4x9ZI Pin Definitions (F429ZI and F439ZI)
// Included by families/stm32f4.h - do not include directly
// Defines pin mappings inside fl:: namespace
// STM32F429ZI/F439ZI - 117 pins (Nucleo-144 form factor)

// IWYU pragma: private, include "platforms/arm/stm32/pins/families/stm32f4.h"

#include "platforms/arm/stm32/pins/core/armpin_template.h"
#include "platforms/arm/stm32/pins/core/gpio_port_init.h"
#include "platforms/arm/stm32/pins/core/pin_macros.h"
#include "platforms/arm/stm32/pins/families/stm32f4.h"

#ifndef _DEFPIN_ARM_F4
  #error "This file must be included via families/stm32f4.h which provides the _DEFPIN_ARM_F4 macro"
#endif

// Port G
_DEFPIN_ARM_F4(0, 9, G);    // PG_9
_DEFPIN_ARM_F4(1, 14, G);   // PG_14
// Port F
_DEFPIN_ARM_F4(2, 15, F);   // PF_15
// Port E
_DEFPIN_ARM_F4(3, 13, E);   // PE_13
// Port F
_DEFPIN_ARM_F4(4, 14, F);   // PF_14
// Port E
_DEFPIN_ARM_F4(5, 11, E);   // PE_11
_DEFPIN_ARM_F4(6, 9, E);    // PE_9
// Port F
_DEFPIN_ARM_F4(7, 13, F);   // PF_13
_DEFPIN_ARM_F4(8, 12, F);   // PF_12
// Port D
_DEFPIN_ARM_F4(9, 15, D);   // PD_15
_DEFPIN_ARM_F4(10, 14, D);  // PD_14
// Port A
_DEFPIN_ARM_F4(11, 7, A);   // PA_7
_DEFPIN_ARM_F4(12, 6, A);   // PA_6
_DEFPIN_ARM_F4(13, 5, A);   // PA_5
// Port B
_DEFPIN_ARM_F4(14, 9, B);   // PB_9
_DEFPIN_ARM_F4(15, 8, B);   // PB_8
// Port C
_DEFPIN_ARM_F4(16, 6, C);   // PC_6
// Port B
_DEFPIN_ARM_F4(17, 15, B);  // PB_15
_DEFPIN_ARM_F4(18, 13, B);  // PB_13
_DEFPIN_ARM_F4(19, 12, B);  // PB_12
// Port A
_DEFPIN_ARM_F4(20, 15, A);  // PA_15
// Port C
_DEFPIN_ARM_F4(21, 7, C);   // PC_7
// Port B
_DEFPIN_ARM_F4(22, 5, B);   // PB_5
_DEFPIN_ARM_F4(23, 3, B);   // PB_3
// Port A
_DEFPIN_ARM_F4(24, 4, A);   // PA_4
// Port B
_DEFPIN_ARM_F4(25, 4, B);   // PB_4
_DEFPIN_ARM_F4(26, 6, B);   // PB_6
_DEFPIN_ARM_F4(27, 2, B);   // PB_2
// Port D
_DEFPIN_ARM_F4(28, 13, D);  // PD_13
_DEFPIN_ARM_F4(29, 12, D);  // PD_12
_DEFPIN_ARM_F4(30, 11, D);  // PD_11
// Port E
_DEFPIN_ARM_F4(31, 2, E);   // PE_2
// Port A
_DEFPIN_ARM_F4(32, 0, A);   // PA_0
// Port B
_DEFPIN_ARM_F4(33, 0, B);   // PB_0
// Port E
_DEFPIN_ARM_F4(34, 0, E);   // PE_0
// Port B
_DEFPIN_ARM_F4(35, 11, B);  // PB_11
_DEFPIN_ARM_F4(36, 10, B);  // PB_10
// Port E
_DEFPIN_ARM_F4(37, 15, E);  // PE_15
_DEFPIN_ARM_F4(38, 14, E);  // PE_14
_DEFPIN_ARM_F4(39, 12, E);  // PE_12
_DEFPIN_ARM_F4(40, 10, E);  // PE_10
_DEFPIN_ARM_F4(41, 7, E);   // PE_7
_DEFPIN_ARM_F4(42, 8, E);   // PE_8
// Port C
_DEFPIN_ARM_F4(43, 8, C);   // PC_8
_DEFPIN_ARM_F4(44, 9, C);   // PC_9
_DEFPIN_ARM_F4(45, 10, C);  // PC_10
_DEFPIN_ARM_F4(46, 11, C);  // PC_11
_DEFPIN_ARM_F4(47, 12, C);  // PC_12
// Port D
_DEFPIN_ARM_F4(48, 2, D);   // PD_2
// Port G
_DEFPIN_ARM_F4(49, 2, G);   // PG_2
_DEFPIN_ARM_F4(50, 3, G);   // PG_3
// Port D
_DEFPIN_ARM_F4(51, 7, D);   // PD_7
_DEFPIN_ARM_F4(52, 6, D);   // PD_6
_DEFPIN_ARM_F4(53, 5, D);   // PD_5
_DEFPIN_ARM_F4(54, 4, D);   // PD_4
_DEFPIN_ARM_F4(55, 3, D);   // PD_3
// Port E (duplicate D56 is PE_2, already defined at D31)
_DEFPIN_ARM_F4(56, 2, E);   // PE_2 (duplicate)
_DEFPIN_ARM_F4(57, 4, E);   // PE_4
_DEFPIN_ARM_F4(58, 5, E);   // PE_5
_DEFPIN_ARM_F4(59, 6, E);   // PE_6
_DEFPIN_ARM_F4(60, 3, E);   // PE_3
// Port F
_DEFPIN_ARM_F4(61, 8, F);   // PF_8
_DEFPIN_ARM_F4(62, 7, F);   // PF_7
_DEFPIN_ARM_F4(63, 9, F);   // PF_9
// Port G
_DEFPIN_ARM_F4(64, 1, G);   // PG_1
_DEFPIN_ARM_F4(65, 0, G);   // PG_0
// Port D
_DEFPIN_ARM_F4(66, 1, D);   // PD_1
_DEFPIN_ARM_F4(67, 0, D);   // PD_0
// Port F
_DEFPIN_ARM_F4(68, 0, F);   // PF_0
_DEFPIN_ARM_F4(69, 1, F);   // PF_1
_DEFPIN_ARM_F4(70, 2, F);   // PF_2
// Port A (duplicate D71 is PA_7, already defined at D11)
_DEFPIN_ARM_F4(71, 7, A);   // PA_7 (duplicate)
// Pin 72: NC (not connected) - do not use
// _DEFPIN_ARM_F4(72, 0, A); // NC (not connected)
// Port B
_DEFPIN_ARM_F4(73, 7, B);   // PB_7
_DEFPIN_ARM_F4(74, 14, B);  // PB_14
// Port C
_DEFPIN_ARM_F4(75, 13, C);  // PC_13
// Port D
_DEFPIN_ARM_F4(76, 9, D);   // PD_9
_DEFPIN_ARM_F4(77, 8, D);   // PD_8
// Port A
_DEFPIN_ARM_F4(78, 3, A);   // PA_3
// Port C
_DEFPIN_ARM_F4(79, 0, C);   // PC_0
_DEFPIN_ARM_F4(80, 3, C);   // PC_3
// Port F
_DEFPIN_ARM_F4(81, 3, F);   // PF_3
_DEFPIN_ARM_F4(82, 5, F);   // PF_5
_DEFPIN_ARM_F4(83, 10, F);  // PF_10
// Port B
_DEFPIN_ARM_F4(84, 1, B);   // PB_1
// Port C
_DEFPIN_ARM_F4(85, 2, C);   // PC_2
// Port F
_DEFPIN_ARM_F4(86, 4, F);   // PF_4
_DEFPIN_ARM_F4(87, 6, F);   // PF_6
// Port A
_DEFPIN_ARM_F4(88, 1, A);   // PA_1
_DEFPIN_ARM_F4(89, 2, A);   // PA_2
_DEFPIN_ARM_F4(90, 8, A);   // PA_8
_DEFPIN_ARM_F4(91, 9, A);   // PA_9
_DEFPIN_ARM_F4(92, 10, A);  // PA_10
_DEFPIN_ARM_F4(93, 11, A);  // PA_11
_DEFPIN_ARM_F4(94, 12, A);  // PA_12
_DEFPIN_ARM_F4(95, 13, A);  // PA_13
_DEFPIN_ARM_F4(96, 14, A);  // PA_14
// Port C
_DEFPIN_ARM_F4(97, 1, C);   // PC_1
_DEFPIN_ARM_F4(98, 4, C);   // PC_4
_DEFPIN_ARM_F4(99, 5, C);   // PC_5
_DEFPIN_ARM_F4(100, 14, C); // PC_14
_DEFPIN_ARM_F4(101, 15, C); // PC_15
// Port D
_DEFPIN_ARM_F4(102, 10, D); // PD_10
// Port E
_DEFPIN_ARM_F4(103, 1, E);  // PE_1
// Port F
_DEFPIN_ARM_F4(104, 11, F); // PF_11
// Port G
_DEFPIN_ARM_F4(105, 4, G);  // PG_4
_DEFPIN_ARM_F4(106, 5, G);  // PG_5
_DEFPIN_ARM_F4(107, 6, G);  // PG_6
_DEFPIN_ARM_F4(108, 7, G);  // PG_7
_DEFPIN_ARM_F4(109, 8, G);  // PG_8
_DEFPIN_ARM_F4(110, 10, G); // PG_10
_DEFPIN_ARM_F4(111, 11, G); // PG_11
_DEFPIN_ARM_F4(112, 12, G); // PG_12
_DEFPIN_ARM_F4(113, 13, G); // PG_13
_DEFPIN_ARM_F4(114, 15, G); // PG_15
// Port H
_DEFPIN_ARM_F4(115, 0, H);  // PH_0
_DEFPIN_ARM_F4(116, 1, H);  // PH_1
