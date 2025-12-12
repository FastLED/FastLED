// ValidationConfig.h - Validation test matrix configuration
// This file defines the test matrix dimensions (drivers, lanes, strip sizes)
// Include this BEFORE Common.h in all files to ensure consistent configuration

#pragma once

// ============================================================================
// RMT TX Test: Single lane, 300 LEDs (Testing RMT capture capability)
// ============================================================================

// Driver selection
// #define JUST_PARLIO
#define JUST_RMT

// Lane range (MUST be defined BEFORE Common.h)
#define MIN_LANES 1
#define MAX_LANES 1

// Strip size selection
// #define JUST_SMALL_STRIPS
#define JUST_LARGE_STRIPS

// Strip size constants (MUST be defined BEFORE Common.h)
#define SHORT_STRIP_SIZE 10
#define LONG_STRIP_SIZE 300
