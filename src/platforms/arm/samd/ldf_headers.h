#pragma once

// IWYU pragma: private

// ok no namespace fl

/// @file platforms/arm/samd/ldf_headers.h
/// SAMD PlatformIO Library Dependency Finder (LDF) hints
///
/// This file contains #if 0 blocks with library includes to hint dependencies
/// to PlatformIO's LDF scanner without actually compiling the code.

// SPI: required, and required unconditionally.
//
// platforms/spi_device_proxy.h routes SAMD into
// platforms/arm/sam/spi_device_proxy.h -> fastspi_arm_sam.h, which does
// `#include <SPI.h>` under `#if defined(FL_IS_SAMD21) || defined(FL_IS_SAMD51)`.
// The hint goes here rather than in a board's lib_deps so that any consumer of
// the library gets a working SAMD build, not just this repo's CI matrix.
//
// This was invisible until FastLED#4011: FL_IS_SAMD21/FL_IS_SAMD51 never
// evaluated true, so the whole SAMD SPI stack -- and the FL_IS_SAMD gate in
// platforms/ldf_headers.h that reaches this file -- was unreachable. Fixing
// detection made both live, and the build failed with
// `fastspi_arm_sam.h:17:10: fatal error: SPI.h: No such file or directory`.
//
// Unconditional on purpose. The include in fastspi_arm_sam.h is conditional,
// but the LDF scanner does not evaluate conditionals, so narrowing the hint
// would buy nothing and risk it being skipped. Cost is bounded: SPI is a small
// core library. Contrast the ESP32 hints, which deliberately refuse WiFi.h and
// friends because their global constructors become GC roots and defeat
// --gc-sections. SPI's SPIClass instance is the same class of cost, which is
// why FastLED#4016 tracks routing SAMD to a backend that does not need the
// Arduino library at all.
#if 0
// IWYU pragma: begin_keep
#include <SPI.h>
// IWYU pragma: end_keep
#endif
