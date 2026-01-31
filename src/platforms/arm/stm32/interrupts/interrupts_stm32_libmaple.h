// ok no namespace fl
#pragma once

/// @file interrupts_stm32_libmaple.h
/// Interrupt control for Arduino_STM32 (Roger Clark libmaple core)
/// Legacy core - "break/fix level only", "adequate for hobby use"
///
/// Interrupt control is provided by fl::interruptsDisable() and
/// fl::interruptsEnable() in fl/isr.h (implemented in isr_stm32.hpp)

#include "platforms/is_platform.h"

#ifndef FL_IS_STM32_LIBMAPLE
#error "This header is only for Arduino_STM32 (libmaple) core"
#endif

// Interrupt control: use fl::interruptsDisable() / fl::interruptsEnable()
