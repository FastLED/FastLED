// ValidationConfig.h - Validation test matrix configuration
// This file defines the test matrix dimensions (drivers, lanes, strip sizes)
// Include this BEFORE Common.h in all files to ensure consistent configuration

#pragma once

// ============================================================================
// PARLIO Multi-Buffer Test: Single lane, 1000 LEDs (Phase 1 Validation)
// ============================================================================

// Driver selection
#define JUST_PARLIO  // ITERATION 1: Re-enabled for PARLIO investigation
// #define JUST_RMT  // ITERATION 4: Verified - RX decoder works correctly (99.6% accuracy)

// Lane range (MUST be defined BEFORE Common.h)
#define MIN_LANES 1
#define MAX_LANES 1

// Strip size selection
// #define JUST_SMALL_STRIPS  // ITERATION 1: Phase 0 - Sanity check with small strips (10-30 LEDs)
#define JUST_LARGE_STRIPS  // ITERATION 6: Phase 2 - Testing DMA gaps with 1000 LEDs (24 buffers)

// Strip size constants (MUST be defined BEFORE Common.h)
#define SHORT_STRIP_SIZE 10
#define LONG_STRIP_SIZE 1000  // 1000 led's will exceed any attempts by the agent to put into one buffer.
