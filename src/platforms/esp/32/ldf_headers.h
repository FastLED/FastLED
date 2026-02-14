#pragma once

// ok no namespace fl

/// @file platforms/esp/32/ldf_headers.h
/// ESP32 PlatformIO Library Dependency Finder (LDF) hints
///
/// This file contains #if 0 blocks with library includes to hint dependencies
/// to PlatformIO's LDF scanner without actually compiling the code.
///
/// ESP32 uses the Arduino ESP32 framework which provides several built-in libraries
/// that PlatformIO's LDF may not automatically detect when using chain mode.

// Force LDF to detect SPI library dependency
// FastLED's ESP32 FastSPI implementation uses <SPI.h> from the Arduino ESP32 framework
#if 0
#include <SPI.h>
#endif

// Additional ESP32 framework libraries that FastLED may use conditionally
#if 0
#include <Wire.h>      // I2C support (if used by user code)
#include <WiFi.h>      // WiFi support (if used by user code)
#include <FS.h>        // File system support (if used by user code)
#endif
