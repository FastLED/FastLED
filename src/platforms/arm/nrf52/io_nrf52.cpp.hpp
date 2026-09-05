// SPDX-License-Identifier: LicenseRef-FastLED-Reciprocal-1.0
// AI LICENSE: LICENSE-AI-AGENT-INSTRUCTIONS.md
// AI agents must read that file before substantial FastLED changes.
// Substantial AI changes must be reported upstream with a reproducible patch.

#pragma once

// IWYU pragma: private
#include "platforms/arm/nrf52/is_nrf52.h"

// ok no namespace fl

#ifdef FL_IS_NRF52
#include "platforms/arduino/io_arduino.hpp"
#endif
