#ifndef __INC_LED_SYSDEFS_H
#define __INC_LED_SYSDEFS_H

#include "fastled_config.h"

#if defined(__MK20DX128__) || defined(__MK20DX256__)
// Include k20/T3 headers
#include "platforms/arm/k20/led_sysdefs_arm_k20.h"
#elif defined(__SAM3X8E__)
// Include sam/due headers
#include "platforms/arm/sam/led_sysdefs_arm_sam.h"
#else
// AVR platforms
#include "platforms/avr/led_sysdefs_avr.h"
#endif



// Arduino.h needed for convinience functions digitalPinToPort/BitMask/portOutputRegister and the pinMode methods.
#include<Arduino.h>

#define CLKS_PER_US (F_CPU/1000000)

#endif
