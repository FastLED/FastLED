#pragma once

// IWYU pragma: private

// ok no namespace fl

/// @file platforms/arm/samd/ldf_headers.h
/// SAMD PlatformIO Library Dependency Finder (LDF) hints
///
/// This file contains #if 0 blocks with library includes to hint dependencies
/// to PlatformIO's LDF scanner without actually compiling the code.

// No hints needed: SAMD hardware SPI programs the core SERCOM abstraction
// directly and does not depend on Arduino's SPI library. Non-variant pins use
// FastLED's software SPI fallback.
