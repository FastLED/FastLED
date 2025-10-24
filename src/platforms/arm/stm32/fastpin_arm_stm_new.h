#pragma once

// ok no namespace fl
// Main dispatcher for STM32 variant-specific pin definitions
// This file includes the appropriate variant file based on board definitions

#if defined(SPARK)
#include "fastpin_stm32_spark.h"
#elif defined(__STM32F1__) || defined(STM32F1)
#include "fastpin_stm32f1.h"
#else
#error "Please define pin map for this board"
#endif
