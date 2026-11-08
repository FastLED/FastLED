#pragma once

// IWYU pragma: private

// ok no namespace fl

/// @file is_nrf51.h
/// @brief Nordic nRF51 (ARM Cortex-M0) platform detection for FastLED
///
/// The arduino-nRF5 core (framework-arduinonordicnrf5) defines NRF51 for
/// nRF51 boards such as the BBC micro:bit v1. The RFduino and Simblee cores
/// (also nRF51-based) define __RFduino__ / __Simblee__.

#if (defined(NRF51) || defined(__RFduino__) || defined(__Simblee__)) && \
    !defined(NRF52) && !defined(NRF52_SERIES)
#define FL_IS_NRF51
#endif
