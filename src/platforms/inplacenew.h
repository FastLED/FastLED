// ok no namespace fl
#pragma once

// Platform router for placement new operator
// Delegates to platform-specific implementations or standard library

#if defined(__AVR__)
// AVR doesn't have <new> header, needs manual definition
#include "avr/inplacenew.h"
#elif defined(__STM32F1__)
// Roger Clark STM32 has broken/missing placement new
#include "shared/inplacenew.h"
#elif !defined(__has_include)
// Platforms without __has_include support - assume no <new> header
#include "shared/inplacenew.h"
#elif __has_include(<new>)
// Modern platforms with standard library support
#include <new>
#elif __has_include(<new.h>)
// Alternative standard header location
#include <new.h>
#elif __has_include("new.h")
// Local new.h header
#include "new.h"
#else
// Fallback to manual definition
#include "shared/inplacenew.h"
#endif
