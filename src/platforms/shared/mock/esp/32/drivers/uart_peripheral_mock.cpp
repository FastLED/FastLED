/// @file uart_peripheral_mock.cpp
/// @brief Mock UART peripheral implementation for unit testing

#include "uart_peripheral_mock.h"

#if defined(ARDUINO) || defined(ESP_PLATFORM) || defined(ESP32)
#include <Arduino.h>  // ok include - For micros() on Arduino/ESP platforms
#else
#include "platforms/stub/time_stub.h"  // For micros() on host tests
#endif

namespace fl {

//=============================================================================
// Constructor / Destructor
//=============================================================================

UartPeripheralMock::UartPeripheralMock()
    : mConfig(),
      mInitialized(false),
      mBusy(false),
      mCapturedData(),
      mTransmissionDelayUs(0),
      mManualDelaySet(false),
      mLastWriteTimestamp(0),
      mResetExpireTime(0),
      mLastCalculatedResetDuration(0),
      mVirtualTimeEnabled(false),
      mVirtualTime(0) {
}

UartPeripheralMock::~UartPeripheralMock() {
    deinitialize();
}

//=============================================================================
// IUartPeripheral Interface Implementation
//=============================================================================

bool UartPeripheralMock::initialize(const UartConfig& config) {
    if (mInitialized) {
        // Already initialized - deinitialize first
        deinitialize();
    }

    // Validate configuration
    if (config.mBaudRate == 0) {
        return false;  // Invalid baud rate
    }
    if (config.mTxPin < 0) {
        return false;  // Invalid TX pin
    }
    if (config.mStopBits == 0 || config.mStopBits > 2) {
        return false;  // Invalid stop bits (must be 1 or 2)
    }

    // Store configuration
    mConfig = config;
    mInitialized = true;
    mBusy = false;
    mLastWriteTimestamp = 0;

    return true;
}

void UartPeripheralMock::deinitialize() {
    if (!mInitialized) {
        return;  // Already deinitialized
    }

    // Clear state
    mInitialized = false;
    mBusy = false;
    mLastWriteTimestamp = 0;
}

bool UartPeripheralMock::isInitialized() const {
    return mInitialized;
}

bool UartPeripheralMock::writeBytes(const uint8_t* data, size_t length) {
    if (!mInitialized) {
        return false;  // Not initialized
    }
    if (data == nullptr || length == 0) {
        return false;  // Invalid parameters
    }

    // Capture transmitted bytes
    for (size_t i = 0; i < length; ++i) {
        mCapturedData.push_back(data[i]);
    }

    // Calculate realistic transmission delay based on baud rate and byte count
    // UNLESS a manual delay was set via setTransmissionDelay()
    // Pattern from PARLIO: transmission_time_us = (bit_count * 1000000) / clock_freq_hz
    // For UART: transmission_time_us = (byte_count * bits_per_frame * 1000000) / baud_rate
    // bits_per_frame = 1 start bit + 8 data bits + stop bits (10 for 8N1, 11 for 8N2)
    if (!mManualDelaySet) {
        if (mConfig.mBaudRate > 0) {
            const size_t bits_per_frame = 10 + (mConfig.mStopBits == 2 ? 1 : 0);  // 10 (8N1) or 11 (8N2)
            const uint64_t total_bits = static_cast<uint64_t>(length) * bits_per_frame;
            // Calculate transmission time in microseconds
            uint64_t transmission_time_us = (total_bits * 1000000ULL) / mConfig.mBaudRate;
            // Add small overhead for buffer switching (10 microseconds)
            mTransmissionDelayUs = static_cast<uint32_t>(transmission_time_us) + 10;
        } else {
            // Fallback: Use a small default delay if baud rate not configured
            mTransmissionDelayUs = 100;  // 100 microseconds default
        }
    }

    // Mark as busy
    mBusy = true;
    mLastWriteTimestamp = getCurrentTimestamp();
    mResetExpireTime = 0;  // Clear previous reset timer (new transmission starting)

    return true;
}

bool UartPeripheralMock::waitTxDone(uint32_t timeout_ms) {
    if (!mInitialized) {
        return false;  // Not initialized
    }
    if (!mBusy) {
        return true;  // No transmission in progress
    }

    // Simulate transmission timing
    const uint64_t start_time = getCurrentTimestamp();
    const uint64_t timeout_us = timeout_ms * 1000ULL;

    while (mBusy) {
        // Check if transmission is complete
        if (isTransmissionComplete()) {
            mBusy = false;
            return true;
        }

        // Check timeout
        const uint64_t elapsed = getCurrentTimestamp() - start_time;
        if (elapsed >= timeout_us) {
            return false;  // Timeout
        }

        // Busy wait (in real implementation, this would yield to other threads)
        // For mock, we just check completion immediately
    }

    return true;
}

bool UartPeripheralMock::isBusy() const {
    if (!mInitialized) {
        return false;
    }

    // Check reset period FIRST
    const uint64_t now = getCurrentTimestamp();
    if (now < mResetExpireTime) {
        return true;  // Still in reset period (channel draining)
    }

    // Then check if transmission is still in progress
    if (mBusy) {
        return !const_cast<UartPeripheralMock*>(this)->isTransmissionComplete();
    }

    return false;
}

const UartConfig& UartPeripheralMock::getConfig() const {
    return mConfig;
}

//=============================================================================
// Mock-Specific API
//=============================================================================

void UartPeripheralMock::setTransmissionDelay(uint32_t microseconds) {
    mTransmissionDelayUs = microseconds;
    mManualDelaySet = true;
}

void UartPeripheralMock::forceTransmissionComplete() {
    mBusy = false;
    mResetExpireTime = 0;  // Clear reset period
}

void UartPeripheralMock::reset() {
    mInitialized = false;
    mBusy = false;
    mCapturedData.clear();
    mTransmissionDelayUs = 0;
    mManualDelaySet = false;
    mLastWriteTimestamp = 0;
    mResetExpireTime = 0;
    mLastCalculatedResetDuration = 0;
    mVirtualTimeEnabled = false;
    mVirtualTime = 0;
    mConfig = UartConfig();
}

fl::vector<uint8_t> UartPeripheralMock::getCapturedBytes() const {
    return mCapturedData;
}

size_t UartPeripheralMock::getCapturedByteCount() const {
    return mCapturedData.size();
}

void UartPeripheralMock::resetCapturedData() {
    mCapturedData.clear();
}

uint64_t UartPeripheralMock::getLastCalculatedResetDurationUs() const {
    return mLastCalculatedResetDuration;
}

void UartPeripheralMock::setVirtualTimeMode(bool enabled) {
    mVirtualTimeEnabled = enabled;
    if (enabled && mVirtualTime == 0) {
        // Initialize virtual time to a non-zero value to avoid edge cases
        mVirtualTime = 1000;
    }
}

void UartPeripheralMock::advanceTime(uint64_t microseconds) {
    if (mVirtualTimeEnabled) {
        mVirtualTime += microseconds;
    }
}

uint64_t UartPeripheralMock::getVirtualTime() const {
    return mVirtualTimeEnabled ? mVirtualTime : 0;
}

fl::vector<bool> UartPeripheralMock::getWaveformWithFraming() const {
    fl::vector<bool> waveform;
    waveform.reserve(mCapturedData.size() * 10);  // 10 bits per byte (8N1)

    for (uint8_t byte : mCapturedData) {
        // Start bit (always LOW/false)
        waveform.push_back(false);

        // 8 data bits (LSB first)
        for (int i = 0; i < 8; ++i) {
            bool bit = (byte >> i) & 0x01;
            waveform.push_back(bit);
        }

        // Stop bit (always HIGH/true)
        // Note: This handles 8N1 (1 stop bit). For 8N2, we'd add two HIGH bits.
        waveform.push_back(true);
        if (mConfig.mStopBits == 2) {
            waveform.push_back(true);  // Second stop bit for 8N2
        }
    }

    return waveform;
}

bool UartPeripheralMock::verifyStartStopBits() const {
    if (mCapturedData.empty()) {
        return false;  // No data to verify
    }

    // Generate waveform
    fl::vector<bool> waveform = getWaveformWithFraming();

    // Calculate bits per frame (10 for 8N1, 11 for 8N2)
    const size_t bits_per_frame = 10 + (mConfig.mStopBits == 2 ? 1 : 0);

    // Verify each frame
    for (size_t i = 0; i < mCapturedData.size(); ++i) {
        const size_t frame_start = i * bits_per_frame;

        // Check start bit (bit 0 of frame) = LOW (false)
        if (waveform[frame_start] != false) {
            return false;  // Invalid start bit
        }

        // Check first stop bit (bit 9 of frame) = HIGH (true)
        if (waveform[frame_start + 9] != true) {
            return false;  // Invalid stop bit
        }

        // Check second stop bit if 8N2
        if (mConfig.mStopBits == 2) {
            if (waveform[frame_start + 10] != true) {
                return false;  // Invalid second stop bit
            }
        }
    }

    return true;
}

//=============================================================================
// Internal Helpers
//=============================================================================

uint64_t UartPeripheralMock::getCurrentTimestamp() const {
    if (mVirtualTimeEnabled) {
        return mVirtualTime;
    }
    return micros();
}

bool UartPeripheralMock::isTransmissionComplete() const {
    if (!mBusy) {
        return true;
    }

    // If no delay configured, transmission is instant
    if (mTransmissionDelayUs == 0) {
        return true;
    }

    // Check if enough time has elapsed
    const uint64_t now = getCurrentTimestamp();
    const uint64_t elapsed = now - mLastWriteTimestamp;

    if (elapsed >= mTransmissionDelayUs) {
        // Transmission just completed - set reset timer ONCE (only if not already set)
        // WS2812 requires >50us reset period
        // Make reset duration equal to transmission time for more realistic testing
        // This simulates the time needed for the channel to drain/settle
        // Minimum of 50us (WS2812 requirement)
        if (mResetExpireTime == 0) {
            const uint64_t MIN_RESET_DURATION_US = 50;  // Minimum for WS2812
            // Reset period equals transmission time (simulating channel drain time)
            // IMPORTANT: Use the current transmission delay, not a future one
            // (mTransmissionDelayUs can be overwritten by subsequent writeBytes() calls)
            const uint64_t current_tx_delay = mTransmissionDelayUs;
            const uint64_t reset_duration = (current_tx_delay > MIN_RESET_DURATION_US) ?
                                           current_tx_delay : MIN_RESET_DURATION_US;
            const_cast<UartPeripheralMock*>(this)->mResetExpireTime = now + reset_duration;
            const_cast<UartPeripheralMock*>(this)->mLastCalculatedResetDuration = reset_duration;
        }
        return true;
    }

    return false;
}

} // namespace fl
