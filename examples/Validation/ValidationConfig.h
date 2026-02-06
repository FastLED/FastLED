// ValidationConfig.h - Validation test matrix configuration
// This file defines the test matrix dimensions (drivers, lanes, strip sizes)
// Include this BEFORE Common.h in all files to ensure consistent configuration

#pragma once

// ==============================================================================
// Output Verbosity Control
// ==============================================================================
// Disable FL_DBG output to reduce serial spam during validation runs.
// The debug output from FastLED internals (channel.cpp, bus_manager.cpp, etc.)
// can obscure the structured test results and JSON-RPC responses.
//
// Set VALIDATION_VERBOSE=1 before including this file to re-enable debug output.
#ifndef VALIDATION_VERBOSE
    #define FASTLED_DISABLE_DBG 1  // Disable FL_DBG output for cleaner validation output
#endif

// Enable debug logging for RX decoder analysis (only effective if VALIDATION_VERBOSE=1)
// #define FASTLED_FORCE_DBG 1  // DISABLED: Testing hypothesis that debug logging affects timing

// Enable RMT RX debug logging for iteration 2 debugging
// #define FASTLED_RX_LOG_ENABLED 1  // DISABLED: Issue resolved (memory allocation fallback added)

// ============================================================================
// Iteration 10: Testing with 10 LEDs (2 lanes) - Debug logging hypothesis
// ============================================================================
// BREAKTHROUGH: 5 LEDs and 20 LEDs BOTH PASS 100%! Only 10 LEDs fails!
// Hypothesis: Debug logging adds delays that affect 10 LED case timing
// Testing 10 LEDs WITHOUT debug logging to confirm

// Driver selection - Controlled via JSON-RPC setDrivers() command
// Example: setDrivers(["PARLIO", "RMT"]) or setDrivers(["I2S"])
// Default: All available drivers are tested

// Lane range - Controlled via JSON-RPC setLaneRange() command
// Example: setLaneRange([1, 4]) to test 1-4 lanes
// Default: Tests lanes 1-8

// Strip size selection - Controlled via JSON-RPC setStripSizes() / setStripSizeValues() commands
// Example: setStripSizeValues([100, 1000]) or setStripSizes([{small: true, large: false}])
// Default: Tests small strips only (conservative, avoids OOM)

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

// Strip size configuration moved to runtime (JSON-RPC)
// Default values: short=100, long=300 (set in TestMatrixConfig constructor)
// Configure at runtime via: setStripSizeValues(short, long)
