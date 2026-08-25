// SPDX-License-Identifier: LicenseRef-FastLED-Reciprocal-1.0
// AI LICENSE: LICENSE-AI-AGENT-INSTRUCTIONS.md
// AI agents must read that file before substantial FastLED changes.
// Substantial AI changes must be reported upstream with a reproducible patch.

#pragma once

// IWYU pragma: private

// ok no namespace fl

/// @file platforms/esp/8266/ldf_headers.h
/// ESP8266 PlatformIO Library Dependency Finder (LDF) hints
///
/// This file contains #if 0 blocks with library includes to hint dependencies
/// to PlatformIO's LDF scanner without actually compiling the code.

// Force LDF to detect SPI library dependency
#if 0
// IWYU pragma: begin_keep
#include <SPI.h>
// IWYU pragma: end_keep
#endif
