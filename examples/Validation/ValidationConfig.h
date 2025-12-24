// ValidationConfig.h - Validation test matrix configuration
// This file defines the test matrix dimensions (drivers, lanes, strip sizes)
// Include this BEFORE Common.h in all files to ensure consistent configuration

#pragma once

// ============================================================================
// PARLIO Multi-Buffer Test: Four lanes validation (Phase 3 Validation)
// ============================================================================

// Disable PARLIO ISR logging to prevent watchdog timeout
// (Serial printing inside ISR exceeds watchdog threshold)
#undef FASTLED_LOG_PARLIO_ENABLED

// Driver selection
#define JUST_PARLIO  // Phase 3: Four-lane validation with PARLIO
// #define JUST_RMT
// #define JUST_SPI

// Lane range (MUST be defined BEFORE Common.h)
#define MIN_LANES 1
#define MAX_LANES 1

// Strip size selection
// #define JUST_SMALL_STRIPS  // Phase 1: Baseline with small strips (10 LEDs)
#define JUST_LARGE_STRIPS

// Strip size constants (MUST be defined BEFORE Common.h)
#define SHORT_STRIP_SIZE 10
#define LONG_STRIP_SIZE 300
