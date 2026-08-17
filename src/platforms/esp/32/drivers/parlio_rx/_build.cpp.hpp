// IWYU pragma: private

/// @file _build.hpp
/// @brief Unity build header for platforms\esp\32\drivers\parlio_rx/ directory
///
/// PARLIO-RX 1-bit oversampling capture backend (FastLED#3586).
/// SoCs with a PARLIO RX unit only (ESP32-C6 et al); gc-sections drops
/// the TU when no sketch selects the PARLIO_RX backend.

#include "platforms/esp/32/drivers/parlio_rx/parlio_rx_sampler.cpp.hpp"
