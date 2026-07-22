#pragma once
// ok no namespace fl

// Nucleo G070RB Pin Definitions
// Matches STMicroelectronics variant_NUCLEO_G070RB.cpp pin map
// Included by families/stm32g0.h - do not include directly

// IWYU pragma: private, include "platforms/arm/stm32/pins/families/stm32g0.h"

#include "platforms/arm/stm32/pins/core/armpin_template.h"
#include "platforms/arm/stm32/pins/core/gpio_port_init.h"
#include "platforms/arm/stm32/pins/core/pin_macros.h"
#include "platforms/arm/stm32/pins/families/stm32g0.h"

// Arduino Digital Pins D0-D15
_DEFPIN_ARM_G0(0, 5, C);   // PC_5  (D0)
_DEFPIN_ARM_G0(1, 4, C);   // PC_4  (D1)
_DEFPIN_ARM_G0(2, 10, A);  // PA_10 (D2)
_DEFPIN_ARM_G0(3, 3, B);   // PB_3  (D3)
_DEFPIN_ARM_G0(4, 5, B);   // PB_5  (D4)
_DEFPIN_ARM_G0(5, 4, B);   // PB_4  (D5)
_DEFPIN_ARM_G0(6, 14, B);  // PB_14 (D6)
_DEFPIN_ARM_G0(7, 8, A);   // PA_8  (D7)
_DEFPIN_ARM_G0(8, 9, A);   // PA_9  (D8)
_DEFPIN_ARM_G0(9, 7, C);   // PC_7  (D9)
_DEFPIN_ARM_G0(10, 0, B);  // PB_0  (D10)
_DEFPIN_ARM_G0(11, 7, A);  // PA_7  (D11)
_DEFPIN_ARM_G0(12, 6, A);  // PA_6  (D12)
_DEFPIN_ARM_G0(13, 5, A);  // PA_5  (D13 / LED_BUILTIN)
_DEFPIN_ARM_G0(14, 9, B);  // PB_9  (D14)
_DEFPIN_ARM_G0(15, 8, B);  // PB_8  (D15)

// Morpho Headers & Onboard Peripheral Pins (16..52)
_DEFPIN_ARM_G0(16, 10, C); // PC_10
_DEFPIN_ARM_G0(17, 12, C); // PC_12
_DEFPIN_ARM_G0(18, 14, A); // PA_14 (SWD)
_DEFPIN_ARM_G0(19, 0, D);  // PD_0
_DEFPIN_ARM_G0(20, 3, D);  // PD_3
_DEFPIN_ARM_G0(21, 13, A); // PA_13 (SWD)
_DEFPIN_ARM_G0(22, 4, D);  // PD_4
_DEFPIN_ARM_G0(23, 15, A); // PA_15
_DEFPIN_ARM_G0(24, 7, B);  // PB_7
_DEFPIN_ARM_G0(25, 13, C); // PC_13 (USER_BTN)
_DEFPIN_ARM_G0(26, 14, C); // PC_14
_DEFPIN_ARM_G0(27, 15, C); // PC_15
_DEFPIN_ARM_G0(28, 0, F);  // PF_0
_DEFPIN_ARM_G0(29, 1, F);  // PF_1
_DEFPIN_ARM_G0(30, 2, C);  // PC_2
_DEFPIN_ARM_G0(31, 3, C);  // PC_3
_DEFPIN_ARM_G0(32, 11, C); // PC_11
_DEFPIN_ARM_G0(33, 2, D);  // PD_2
_DEFPIN_ARM_G0(34, 1, D);  // PD_1
_DEFPIN_ARM_G0(35, 5, D);  // PD_5
_DEFPIN_ARM_G0(36, 9, C);  // PC_9
_DEFPIN_ARM_G0(37, 8, C);  // PC_8
_DEFPIN_ARM_G0(38, 6, C);  // PC_6
_DEFPIN_ARM_G0(39, 3, A);  // PA_3 (ST-LINK RX)
_DEFPIN_ARM_G0(40, 6, D);  // PD_6
_DEFPIN_ARM_G0(41, 11, A); // PA_11
_DEFPIN_ARM_G0(42, 12, A); // PA_12
_DEFPIN_ARM_G0(43, 1, C);  // PC_1
_DEFPIN_ARM_G0(44, 0, C);  // PC_0
_DEFPIN_ARM_G0(45, 2, B);  // PB_2
_DEFPIN_ARM_G0(46, 6, B);  // PB_6
_DEFPIN_ARM_G0(47, 15, B); // PB_15
_DEFPIN_ARM_G0(48, 10, B); // PB_10
_DEFPIN_ARM_G0(49, 13, B); // PB_13
_DEFPIN_ARM_G0(50, 2, A);  // PA_2 (ST-LINK TX)
_DEFPIN_ARM_G0(51, 8, D);  // PD_8
_DEFPIN_ARM_G0(52, 9, D);  // PD_9

// Arduino Analog Pins A0-A5 (pins 53..58)
_DEFPIN_ARM_G0(53, 0, A);  // A0 / PA_0
_DEFPIN_ARM_G0(54, 1, A);  // A1 / PA_1
_DEFPIN_ARM_G0(55, 4, A);  // A2 / PA_4
_DEFPIN_ARM_G0(56, 1, B);  // A3 / PB_1
_DEFPIN_ARM_G0(57, 11, B); // A4 / PB_11
_DEFPIN_ARM_G0(58, 12, B); // A5 / PB_12
