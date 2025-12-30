// ValidationConfig.h - Validation test matrix configuration
// This file defines the test matrix dimensions (drivers, lanes, strip sizes)
// Include this BEFORE Common.h in all files to ensure consistent configuration

#pragma once

// ============================================================================
// PARLIO Multi-Buffer Test: Baseline 2-lane test (Phase 0)
// ============================================================================

// Driver selection
#define JUST_PARLIO  // Phase 0: Baseline with 3 buffers, 2 lanes
// #define JUST_RMT
// #define JUST_SPI

// Lane range (MUST be defined BEFORE Common.h)
#define MIN_LANES 2  // Phase 0: Baseline 2-lane test
#define MAX_LANES 2  // Phase 0: Baseline 2-lane test

// Strip size selection
// #define JUST_SMALL_STRIPS  // Phase 1: Baseline with small strips (10 LEDs)
#define JUST_LARGE_STRIPS

// Strip size constants (MUST be defined BEFORE Common.h)
#define SHORT_STRIP_SIZE 10
#define LONG_STRIP_SIZE 3000
