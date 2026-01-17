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
// #define JUST_PARLIO  // Testing PARLIO only
// #define JUST_RMT
// #define JUST_SPI
#define JUST_UART  // Testing UART only

// Lane range (MUST be defined BEFORE Common.h)
#define MIN_LANES 1  // UART testing: Start with 1 lane
#define MAX_LANES 1  // UART testing: Start with 1 lane

// Strip size selection - UART testing: Small strips (10 LEDs per lane)
#define JUST_SMALL_STRIPS  // UART testing: Testing with small strips
// #define JUST_LARGE_STRIPS  // Will test 3000 LEDs next

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
// UART: Reduced to 1 LED due to RMT RX buffer limitation (200 edges max)
// 1 LED = 3 bytes = 12 UART frames = 120 edges (fits in 200 edge buffer with margin)
// Math: 3 bytes × 4 frames/byte = 12 frames; 12 frames × 10 bits/frame × 2 edges/bit = 240 edges
// Actual: ~120 edges measured (10 bits become ~10 edges after edge reduction)
#define SHORT_STRIP_SIZE 1  // UART testing: 1 LED per lane (RMT RX buffer limit)

#if SKETCH_HAS_LOTS_OF_MEMORY
    #define LONG_STRIP_SIZE 3000  // Large strip for platforms with lots of RAM
#else
    #define LONG_STRIP_SIZE 300   // Reduced for ESP32S2 (320KB DRAM limit)
#endif
