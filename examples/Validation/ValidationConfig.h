// ValidationConfig.h - Validation test matrix configuration
// This file defines the test matrix dimensions (drivers, lanes, strip sizes)
// Include this BEFORE Common.h in all files to ensure consistent configuration

#pragma once

// ============================================================================
// PARLIO LSB vs MSB Mode Comparison - 10 LEDs, 1 lane (baseline configuration)
// ============================================================================

// Driver selection
#define JUST_PARLIO  // Testing PARLIO only
// #define JUST_RMT
// #define JUST_SPI

// Lane range (MUST be defined BEFORE Common.h)
#define MIN_LANES 1  // Single lane testing (baseline)
#define MAX_LANES 1  // Single lane testing (baseline)

// Strip size selection - Baseline testing
// #define JUST_LARGE_STRIPS
#define JUST_SMALL_STRIPS  // Baseline testing (10 LEDs)

// Strip size constants (MUST be defined BEFORE Common.h)
#define SHORT_STRIP_SIZE 10  // Baseline from DONE.md
#define LONG_STRIP_SIZE 3000  // (Not used, testing JUST_SMALL_STRIPS)
