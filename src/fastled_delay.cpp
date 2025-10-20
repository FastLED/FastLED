/// @file fastled_delay.cpp
/// Implementation file for FastLED delay utilities

/// Disables pragma messages and warnings
#define FASTLED_INTERNAL
#include "fl/fastled.h"
#include "fastled_delay.h"

// This file primarily serves to:
// 1. Validate that fastled_delay.h compiles correctly with minimal dependencies
// 2. Ensure micros() and platform-specific implementations are available
// 3. Provide a compilation unit for any future non-template implementations
