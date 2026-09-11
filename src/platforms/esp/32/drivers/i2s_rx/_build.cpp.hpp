// SPDX-License-Identifier: LicenseRef-FastLED-Reciprocal-1.0
// AI LICENSE: LICENSE-AI-AGENT-INSTRUCTIONS.md
// AI agents must read that file before substantial FastLED changes.
// Substantial AI changes must be reported upstream with a reproducible patch.

// IWYU pragma: private

/// @file _build.hpp
/// @brief Unity build header for platforms\esp\32\drivers\i2s_rx/ directory
///
/// I2S-RX 1-bit oversampling capture backend (FastLED#3576 Phase 3).
/// Classic ESP32 only; gc-sections drops the TU when no sketch selects
/// the I2S_RX backend.

#include "platforms/esp/32/drivers/i2s_rx/i2s_rx_sampler.cpp.hpp"
