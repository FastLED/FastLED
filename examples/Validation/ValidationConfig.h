// ValidationConfig.h - Validation test matrix configuration
// This file defines the test matrix dimensions (drivers, lanes, strip sizes)
// Include this BEFORE Common.h in all files to ensure consistent configuration

#pragma once

// ============================================================================
// PARLIO Single-Lane Test: Phase 1 Iteration 6 - Scale Isolation @ 10 LEDs
// ============================================================================

// Driver selection
#define JUST_PARLIO  // Testing PARLIO only
// #define JUST_RMT
// #define JUST_SPI

// Lane range (MUST be defined BEFORE Common.h)
#define MIN_LANES 1  // Phase 1: Single lane testing
#define MAX_LANES 1  // Phase 1: Single lane testing

// Strip size selection - Phase 1 Iteration 6: Isolate scale issue (revert to 10 LEDs)
// #define JUST_LARGE_STRIPS
#define JUST_SMALL_STRIPS  // Phase 1 Iteration 6: Test padding fix at baseline scale (10 LEDs)

// Strip size constants (MUST be defined BEFORE Common.h)
#define SHORT_STRIP_SIZE 10  // Phase 1 Iteration 6: Verify padding eliminates boundary bit-flips at small scale
#define LONG_STRIP_SIZE 3000  // (Not used in Iteration 6, testing JUST_SMALL_STRIPS)
