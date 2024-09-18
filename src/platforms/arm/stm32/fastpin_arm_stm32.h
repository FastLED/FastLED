#ifndef __FASTPIN_ARM_STM32_H
#define __FASTPIN_ARM_STM32_H

#if defined(ARDUINO_BLUEPILL_F103C8)
#include "fastpin_arm_stm_bluepill.h"
#else
// Everything else.
#include "fastpin_arm_stm_generic.h"
#endif

#endif // __INC_FASTPIN_ARM_STM32
