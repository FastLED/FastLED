/// @file Esp32P4Parlio.ino
/// @brief ESP32-P4/S3 PARLIO parallel driver example
///
/// This example demonstrates the FastLED PARLIO driver for ESP32-P4 and ESP32-S3,
/// which enables high-performance parallel LED control using hardware DMA.
///
/// Features:
/// - Control up to 16 LED strips simultaneously in parallel
/// - Hardware-accelerated WS2812 timing generation
/// - Near-zero CPU usage during LED updates (DMA handles transmission)
/// - Internal 3.2 MHz clock (no external clock pin required)
/// - Automatic chunking for large LED counts
///
/// Hardware Requirements:
/// - ESP32-P4 or ESP32-S3 development board
/// - WS2812/WS2812B LED strips (or compatible 800kHz RGB LEDs)
/// - Adequate power supply (5V, ~60mA per LED at full brightness)
///
/// Performance:
/// - 256 LEDs × 4 strips: ~130 FPS
/// - 1000 LEDs × 8 strips: ~30 FPS
/// - <5% CPU usage during updates
///
/// Note: This example uses 4 parallel strips with 256 LEDs each.
/// Adjust NUM_STRIPS and NUM_LEDS_PER_STRIP in Esp32P4Parlio.h to match your setup.

// @filter: (target is -DESP32P4)

#include "Esp32P4Parlio.h"
