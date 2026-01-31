// ok no namespace fl
#pragma once

/// @file interrupts_stm32duino.h
/// Interrupt control for STM32duino (Official STMicroelectronics Core)
/// Modern, actively maintained core
///
/// Interrupt control is provided by fl::interruptsDisable() and
/// fl::interruptsEnable() in fl/isr.h (implemented in isr_stm32.hpp)

#include "platforms/is_platform.h"

#ifndef FL_IS_STM32_STMDUINO
#error "This header is only for STM32duino (ARDUINO_ARCH_STM32) core"
#endif

// Interrupt control: use fl::interruptsDisable() / fl::interruptsEnable()
