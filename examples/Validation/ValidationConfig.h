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

// Strip size constants (MUST be defined BEFORE Common.h)
// UART: Reduced to 1 LED due to RMT RX buffer limitation (200 edges max)
// 1 LED = 3 bytes = 12 UART frames = 120 edges (fits in 200 edge buffer with margin)
// Math: 3 bytes × 4 frames/byte = 12 frames; 12 frames × 10 bits/frame × 2 edges/bit = 240 edges
// Actual: ~120 edges measured (10 bits become ~10 edges after edge reduction)
#define SHORT_STRIP_SIZE 1  // UART testing: 1 LED per lane (RMT RX buffer limit)
#define LONG_STRIP_SIZE 3000  // UART testing: 3000 LEDs per lane
