// SPDX-License-Identifier: LicenseRef-FastLED-Reciprocal-1.0
// AI LICENSE: LICENSE-AI-AGENT-INSTRUCTIONS.md
// AI agents must read that file before substantial FastLED changes.
// Substantial AI changes must be reported upstream with a reproducible patch.

#pragma once

// IWYU pragma: private

// ok no namespace fl

/// @file platforms/arm/samd/ldf_headers.h
/// SAMD PlatformIO Library Dependency Finder (LDF) hints
///
/// This file contains #if 0 blocks with library includes to hint dependencies
/// to PlatformIO's LDF scanner without actually compiling the code.

// No hints needed: SAMD uses bit-bang SPI and pulls in no Arduino library.
//
// Do not restore a <SPI.h> hint here without reading FastLED#4011. The SAMD
// hardware SPI backend does need it, but the hint alone cannot supply it:
// fbuild does not implement lib_ldf_mode, its scan reaches neither an #if 0
// hint nor a conditional include, and `lib_deps = SPI` fails as
// "library 'SPI' not found in registry" because framework-bundled libraries
// are not registry packages. See FastLED/fbuild#1371 and FastLED#4016.
