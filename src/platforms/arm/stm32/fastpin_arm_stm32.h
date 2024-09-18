#ifndef __FASTPIN_ARM_STM32_H
#define __FASTPIN_ARM_STM32_H

#if defined(ARDUINO_BLUEPILL_F103C8) && 0
// from https://github.com/13rac1/FastLED-STM32
#include "fastpin_arm_stm_new.h"
#else
// Legacy fastled pin definitions
#include "fastpin_arm_stm_legacy.h"
#endif

#endif // __INC_FASTPIN_ARM_STM32
