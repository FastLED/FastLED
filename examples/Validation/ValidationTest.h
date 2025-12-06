// ValidationTest.h - Helper functions for LED validation testing

#pragma once

#include <FastLED.h>
#include "platforms/esp/32/drivers/rmt_rx/rmt_rx_channel.h"

// Capture transmitted LED data via RX loopback
// - rx_buffer: Buffer to store received bytes
// - pin: GPIO pin to use for RX
// Returns number of bytes captured, or 0 on error
inline size_t capture(fl::span<uint8_t> rx_buffer, uint8_t pin) {
    // Create RX channel
    auto rx_channel = fl::RmtRxChannel::create(pin, 40000000);
    if (!rx_channel) {
        FL_WARN("ERROR: Failed to create RX channel");
        return 0;
    }

    // Clear RX buffer
    fl::memset(rx_buffer.data(), 0, rx_buffer.size());

    // Arm RX receiver
    if (!rx_channel->begin()) {
        FL_WARN("ERROR: Failed to arm RX receiver");
        return 0;
    }

    delayMicroseconds(100);

    // Transmit
    FastLED.show();

    // Wait for RX completion
    auto wait_result = rx_channel->wait(100);
    if (wait_result != fl::RmtRxWaitResult::SUCCESS) {
        FL_WARN("ERROR: RX wait failed");
        FL_WARN("Check: io_loop_back enabled and same pin used for TX/RX");
        return 0;
    }

    // Decode received data directly into rx_buffer
    auto decode_result = rx_channel->decode(fl::CHIPSET_TIMING_WS2812B_RX, rx_buffer);

    if (!decode_result.ok()) {
        FL_WARN("ERROR: Decode failed (error code: " << static_cast<int>(decode_result.error()) << ")");
        return 0;
    }

    return decode_result.value();
}
