// ValidationConfig.h - Validation test matrix configuration
// This file defines the test matrix dimensions (drivers, lanes, strip sizes)
// Include this BEFORE Common.h in all files to ensure consistent configuration

#pragma once

// ============================================================================
// PARLIO Multi-Buffer Test: Single lane, 1000 LEDs (Phase 1 Validation)
// ============================================================================

// Driver selection
#define JUST_PARLIO
// #define JUST_RMT

// Lane range (MUST be defined BEFORE Common.h)
#define MIN_LANES 1
#define MAX_LANES 1

// Strip size selection
// #define JUST_SMALL_STRIPS
#define JUST_LARGE_STRIPS

// Strip size constants (MUST be defined BEFORE Common.h)
#define SHORT_STRIP_SIZE 10
#define LONG_STRIP_SIZE 1000  // 1000 led's will exceed any attempts by the agent to put into one buffer.
