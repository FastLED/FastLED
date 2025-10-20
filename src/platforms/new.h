#pragma once

// Router header for placement new operator - distributes to platform-specific implementations
// This file is NOT in the fl/* namespace to avoid header bloat
// The actual placement new operator is defined in global namespace by platform-specific headers

#if defined(__AVR__)
#include "avr/new.h"
#elif defined(__STM32F1__) || defined(__arm__) || defined(__ARM_ARCH)
#include "arm/new.h"
#else
// Default: use shared/generic implementation
#include "shared/new.h"
#endif
