// src/fl/channels/detail/validation/rx_test.cpp.hpp
//
// RX channel testing implementation

#include "rx_test.h"
#include "fl/rx_device.h"
#include "fl/pin.h"
#include "fl/log.h"
#include "fl/delay.h"

namespace fl {
namespace validation {

inline bool testRxChannel(
    fl::shared_ptr<fl::RxDevice> rx_channel,
    int pin_tx,
    int pin_rx,
    u32 hz,
    size_t buffer_size) {

    FL_WARN("[RX TEST] Testing RX channel with manual GPIO toggle on PIN " << pin_tx);

    // Configure PIN_TX as output using fl::pin functions (temporarily take ownership from FastLED)
    fl::pinMode(pin_tx, fl::PinMode::Output);
    fl::digitalWrite(pin_tx, fl::PinValue::Low);  // Start LOW

    // Generate fast toggles (RMT max signal_range is ~819 μs, so use 100 μs pulses)
    const int num_toggles = 10;
    const int toggle_delay_us = 100;  // 100 μs pulses = 5 kHz square wave

    // Initialize RX channel with signal range for fast GPIO toggles
    // RMT peripheral max is ~819 μs, so use 200 μs (2x our pulse width for safety)
    fl::RxConfig rx_config;
    rx_config.buffer_size = buffer_size;
    rx_config.hz = hz;
    rx_config.signal_range_min_ns = 100;    // min=100ns
    rx_config.signal_range_max_ns = 200000;  // max=200μs
    rx_config.start_low = true;

    if (!rx_channel->begin(rx_config)) {
        FL_ERROR("[RX TEST]: Failed to begin RX channel");
        fl::pinMode(pin_tx, fl::PinMode::Input);  // Release pin
        return false;
    }

    fl::delayMicroseconds(50);  // Let RX stabilize

    // Generate toggle pattern: HIGH -> LOW -> HIGH -> LOW ...
    for (int i = 0; i < num_toggles; i++) {
        fl::digitalWrite(pin_tx, fl::PinValue::High);
        fl::delayMicroseconds(toggle_delay_us);

        fl::digitalWrite(pin_tx, fl::PinValue::Low);
        fl::delayMicroseconds(toggle_delay_us);
    }

    // Wait for RX to finish capturing (timeout = total toggle time + headroom)
    u32 timeout_ms = 100;  // 10 toggles * 200μs = 2ms, use 100ms for safety
    fl::RxWaitResult wait_result = rx_channel->wait(timeout_ms);

    // Release PIN_TX for FastLED use using fl::pin functions
    fl::pinMode(pin_tx, fl::PinMode::Input);

    // Check if we successfully captured data
    if (wait_result != fl::RxWaitResult::SUCCESS) {
        FL_ERROR("[RX TEST]: RX channel wait failed (result: " << static_cast<int>(wait_result) << ")");
        FL_ERROR("[RX TEST]: RX may not be working - check PIN_RX (" << pin_rx << ") and RMT peripheral");
        FL_ERROR("[RX TEST]: If using non-RMT TX, ensure physical jumper from PIN " << pin_tx << " to PIN " << pin_rx);
        return false;
    }

    FL_WARN("[RX TEST] ✓ RX channel captured data from " << num_toggles << " toggles");
    FL_WARN("[RX TEST] ✓ RX channel is functioning correctly");

    return true;
}

}  // namespace validation
}  // namespace fl
