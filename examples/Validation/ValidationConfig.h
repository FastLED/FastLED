// ValidationConfig.h - Validation test matrix configuration
// This file defines the test matrix dimensions (drivers, lanes, strip sizes)
// Include this BEFORE Common.h in all files to ensure consistent configuration

#pragma once

// ============================================================================
// Phase 3 Configuration: PARLIO driver, 4 lanes, 10 LEDs
// ============================================================================

// Driver selection
#define JUST_PARLIO

// Lane range (MUST be defined BEFORE Common.h)
#define MIN_LANES 4
#define MAX_LANES 4

// Strip size selection
#define JUST_SMALL_STRIPS

// Strip size constants (MUST be defined BEFORE Common.h)
#define SHORT_STRIP_SIZE 10
#define LONG_STRIP_SIZE 300
