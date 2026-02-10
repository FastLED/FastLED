// src/fl/channels/detail/validation/rx_test.h
//
// RX channel testing logic - validates RX channel functionality
// via manual GPIO toggle pattern

#pragma once

#include "fl/stl/shared_ptr.h"
#include "fl/int.h"

namespace fl {

// Forward declarations
class RxDevice;

namespace validation {

/// @brief Test RX channel with manual GPIO toggle pattern
/// @param rx_channel RX channel to test
/// @param pin_tx GPIO pin to toggle
/// @param pin_rx GPIO pin for RX
/// @param hz Sampling frequency in Hz
/// @param buffer_size Size of RX buffer in symbols
/// @return true if test passes, false otherwise
/// @note Generates 10 fast toggles (100μs pulses = 5kHz square wave)
/// @note Uses fl::pin API for platform-independent pin control
bool testRxChannel(
    fl::shared_ptr<fl::RxDevice> rx_channel,
    int pin_tx,
    int pin_rx,
    u32 hz,
    size_t buffer_size);

}  // namespace validation
}  // namespace fl
