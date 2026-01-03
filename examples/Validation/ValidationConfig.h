// ValidationConfig.h - Validation test matrix configuration
// This file defines the test matrix dimensions (drivers, lanes, strip sizes)
// Include this BEFORE Common.h in all files to ensure consistent configuration

#pragma once

// ============================================================================
// PARLIO Dual-Lane Test: Phase 2 - Testing 2 lanes @ 3000 LEDs
// ============================================================================

// Driver selection
#define JUST_PARLIO  // Testing PARLIO only
// #define JUST_RMT
// #define JUST_SPI

// Lane range (MUST be defined BEFORE Common.h)
#define MIN_LANES 2  // Phase 2: Dual lane testing
#define MAX_LANES 2  // Phase 2: Dual lane testing

// Strip size selection
// #define JUST_SMALL_STRIPS  // Phase 1: Baseline with small strips (10 LEDs)
#define JUST_LARGE_STRIPS

// Strip size constants (MUST be defined BEFORE Common.h)
#define SHORT_STRIP_SIZE 10
#define LONG_STRIP_SIZE 3000  // Testing with 1 lane × 3000 LEDs to trigger streaming mode
