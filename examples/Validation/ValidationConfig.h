// ValidationConfig.h - Validation test matrix configuration
// This file defines the test matrix dimensions (drivers, lanes, strip sizes)
// Include this BEFORE Common.h in all files to ensure consistent configuration

#pragma once

// ============================================================================
// PARLIO Multi-Buffer Test: 16-lane maximum capacity test
// ============================================================================

// Driver selection
#define JUST_PARLIO  // Testing 16 lanes (maximum PARLIO capacity)
// #define JUST_RMT
// #define JUST_SPI

// Lane range (MUST be defined BEFORE Common.h)
#define MIN_LANES 16  // Testing maximum 16-lane capacity
#define MAX_LANES 16  // Testing maximum 16-lane capacity

// Strip size selection
// #define JUST_SMALL_STRIPS  // Phase 1: Baseline with small strips (10 LEDs)
#define JUST_LARGE_STRIPS

// Strip size constants (MUST be defined BEFORE Common.h)
#define SHORT_STRIP_SIZE 10
#define LONG_STRIP_SIZE 1000  // Reduced from 3000 to avoid memory exhaustion with 16 lanes
