// ValidationConfig.h - Validation test matrix configuration
// This file defines the test matrix dimensions (drivers, lanes, strip sizes)
// Include this BEFORE Common.h in all files to ensure consistent configuration

#pragma once

// ============================================================================
// Phase 3 Configuration: PARLIO TX driver, 2 lanes, 300 LEDs (Regression Testing)
// ============================================================================

// Driver selection
#define JUST_PARLIO
// #define JUST_RMT

// Lane range (MUST be defined BEFORE Common.h)
#define MIN_LANES 2
#define MAX_LANES 2

// Strip size selection
// #define JUST_SMALL_STRIPS
#define JUST_LARGE_STRIPS

// Strip size constants (MUST be defined BEFORE Common.h)
#define SHORT_STRIP_SIZE 10
#define LONG_STRIP_SIZE 300
