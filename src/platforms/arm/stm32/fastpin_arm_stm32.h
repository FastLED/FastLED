#pragma once

#if defined(FASTLED_FORCE_SOFTWARE_PINS)
#warning "Software pin support forced, pin access will be sloightly slower."
#define NO_HARDWARE_PIN_SUPPORT
#undef HAS_HARDWARE_PIN_SUPPORT
#else

#ifndef USE_NEW_STM32_PIN_DEFINITIONS
// Use new pin definitions for all STM32F1 variants to fix pin mapping issues
// The legacy definitions use incorrect Arduino pin numbers instead of STM32 pin names
#if defined(ARDUINO_BLUEPILL_F103C8) || defined(ARDUINO_MAPLE_MINI) || defined(ARDUINO_GENERIC_STM32F103T) || defined(STM32F103TBU6) || defined(STM32F103TB) || defined(__STM32F1__) || defined(STM32F1) || defined(STM32F103C8) || defined(ARDUINO_GENERIC_STM32F103C8) || defined(ARDUINO_GENERIC_F103C8TX)
#if !defined(ARDUINO_HY_TINYSTM103TB)
#define USE_NEW_STM32_PIN_DEFINITIONS
#endif
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
