// ValidationConfig.h - Validation test matrix configuration
// This file defines the test matrix dimensions (drivers, lanes, strip sizes)
// Include this BEFORE Common.h in all files to ensure consistent configuration

#pragma once

// Enable debug logging for RX decoder analysis
// #define FASTLED_FORCE_DBG 1  // DISABLED: Testing hypothesis that debug logging affects timing

// ============================================================================
// Iteration 10: Testing with 10 LEDs (2 lanes) - Debug logging hypothesis
// ============================================================================
// BREAKTHROUGH: 5 LEDs and 20 LEDs BOTH PASS 100%! Only 10 LEDs fails!
// Hypothesis: Debug logging adds delays that affect 10 LED case timing
// Testing 10 LEDs WITHOUT debug logging to confirm

// Driver selection
#define JUST_PARLIO  // Testing PARLIO only
// #define JUST_RMT
// #define JUST_SPI
// #define JUST_UART

// Lane range (MUST be defined BEFORE Common.h)
#define MIN_LANES 2  // Iteration 12: Multi-lane edge case testing
#define MAX_LANES 2  // Iteration 12: Multi-lane edge case testing

// Strip size selection - PARLIO baseline: Small strips (10 LEDs per lane)
#define JUST_SMALL_STRIPS  // Testing with 15 LEDs
// #define JUST_LARGE_STRIPS  // Will test larger strips after baseline

// Memory configuration
// Define SKETCH_HAS_LOTS_OF_MEMORY=1 for platforms with >320KB RAM (ESP32, ESP32S3)
// ESP32S2 has only 320KB DRAM, so we need smaller buffers by default
#ifndef SKETCH_HAS_LOTS_OF_MEMORY
    #if defined(FL_IS_ESP_32S2)
        #define SKETCH_HAS_LOTS_OF_MEMORY 0  // ESP32S2: Limited to 320KB DRAM
    #else
        #define SKETCH_HAS_LOTS_OF_MEMORY 1  // ESP32/S3/C3/C6: More RAM available
    #endif
#endif

// Strip size constants (MUST be defined BEFORE Common.h)
// Iteration 12: Multi-lane regression - 20 LEDs (2 lanes, data_width=2, expansion 16x)
#define SHORT_STRIP_SIZE 20  // Test 20 LEDs with 2 lanes (expect 3 buffers)

#if SKETCH_HAS_LOTS_OF_MEMORY
    #define LONG_STRIP_SIZE 3000  // Large strip for platforms with lots of RAM
#else
    #define LONG_STRIP_SIZE 300   // Reduced for ESP32S2 (320KB DRAM limit)
#endif
