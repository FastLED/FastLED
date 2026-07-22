#pragma once

// IWYU pragma: private

// ok no namespace fl

/// @file platforms/esp/32/ldf_headers.h
/// ESP32 PlatformIO Library Dependency Finder (LDF) hints
///
/// This file contains #if 0 blocks with library includes to hint dependencies
/// to PlatformIO's LDF scanner without actually compiling the code.
///
/// ESP32 uses the Arduino ESP32 framework which provides several built-in libraries
/// that PlatformIO's LDF may not automatically detect when using chain mode.

// SPI: No Arduino SPI.h dependency — the default backend is ESP-IDF's
// driver/spi_master.h (see core/fastspi_esp32_idf.h). The Arduino SPIClass
// backend is a FASTLED_ESP32_SPI_ARDUINO=1 opt-in escape hatch only; users
// who set that flag are expected to also declare their own <SPI.h> LDF need.

// WARNING: Do NOT add WiFi.h, Wire.h, FS.h, or other large framework libraries here.
// PlatformIO's LDF scans #include directives even inside #if 0 blocks, which causes
// the entire library (and its global constructors) to be linked. Global C++ objects
// like the Arduino `WiFi` instance end up in .init_array, creating GC roots that
// prevent --gc-sections from removing ~326 KB of WiFi/lwIP/mbedTLS code — even when
// the user's sketch never uses networking.

// BLE: No Arduino BLE dependency — uses ESP-IDF NimBLE C API directly.
// See drivers/ble/ble_esp32.cpp.hpp for implementation.
