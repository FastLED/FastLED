// ValidationConfig.h - Validation test matrix configuration
// This file defines the test matrix dimensions (drivers, lanes, strip sizes)
// Include this BEFORE Common.h in all files to ensure consistent configuration

#pragma once

// Enable debug logging for RX decoder analysis
#define FASTLED_FORCE_DBG 1

// ============================================================================
// Phase 2a: PARLIO with 2 lanes and 10 LEDs (Small strips)
// ============================================================================
// Multi-lane testing begins: Verify PARLIO can handle 2 parallel lanes correctly.

// Driver selection
#define JUST_PARLIO  // Testing PARLIO only
// #define JUST_RMT
// #define JUST_SPI

// Lane range (MUST be defined BEFORE Common.h)
#define MIN_LANES 2  // Phase 2a: Multi-lane testing (2 lanes)
#define MAX_LANES 2  // Phase 2a: Multi-lane testing (2 lanes)

// Strip size selection - Phase 2a: Small strips (10 LEDs per lane)
#define JUST_SMALL_STRIPS  // Phase 2a: Testing with small strips
// #define JUST_LARGE_STRIPS  // Phase 2b will test 300 LEDs

// Strip size constants (MUST be defined BEFORE Common.h)
#define SHORT_STRIP_SIZE 10  // Phase 2a: 10 LEDs per lane
#define LONG_STRIP_SIZE 300  // Phase 2b: 300 LEDs (maximum working size for PARLIO)
