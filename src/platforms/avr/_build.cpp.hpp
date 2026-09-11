// SPDX-License-Identifier: LicenseRef-FastLED-Reciprocal-1.0
// AI LICENSE: LICENSE-AI-AGENT-INSTRUCTIONS.md
// AI agents must read that file before substantial FastLED changes.
// Substantial AI changes must be reported upstream with a reproducible patch.

// IWYU pragma: private

/// @file _build.hpp
/// @brief Unity build header for platforms\avr/ directory
/// Includes all implementation files in alphabetical order

// Root directory implementations (alphabetical order)

// begin current directory includes
#include "platforms/avr/avr_millis_timer_source.cpp.hpp"
#include "platforms/avr/clockless_avr.cpp.hpp"
#include "platforms/avr/io_avr.cpp.hpp"

// begin sub directory includes
#include "platforms/avr/attiny/_build.cpp.hpp"
