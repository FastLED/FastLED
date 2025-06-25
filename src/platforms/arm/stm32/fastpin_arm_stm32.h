#pragma once

#if defined(FASTLED_FORCE_SOFTWARE_PINS)
#warning "Software pin support forced, pin access will be sloightly slower."
#define NO_HARDWARE_PIN_SUPPORT
#undef HAS_HARDWARE_PIN_SUPPORT
#else

#ifndef USE_NEW_STM32_PIN_DEFINITIONS
#if defined(ARDUINO_BLUEPILL_F103C8) || defined(ARDUINO_MAPLE_MINI) || defined(ARDUINO_GENERIC_STM32F103T) || defined(STM32F103TBU6) || defined(STM32F103TB)
#define USE_NEW_STM32_PIN_DEFINITIONS
#endif
#endif

#if defined(USE_NEW_STM32_PIN_DEFINITIONS)
// from https://github.com/13rac1/FastLED-STM32
#include "fastpin_arm_stm_new.h"
#else
// Legacy fastled pin definitions
#include "fastpin_arm_stm_legacy.h"
#endif

#endif  // FASTLED_FORCE_SOFTWARE_PINS
