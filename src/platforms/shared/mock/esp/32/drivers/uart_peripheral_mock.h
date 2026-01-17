/// @file uart_peripheral_mock.h
/// @brief Mock UART peripheral for unit testing
///
/// This class simulates ESP32 UART hardware behavior for host-based unit tests.
/// It provides:
/// - Byte-level data capture for validation
/// - Waveform extraction with start/stop bits (10-bit frames for 8N1)
/// - Start/stop bit verification
/// - Transmission timing simulation
/// - State inspection for debugging
///
/// ## Design Philosophy
///
/// The mock implementation is designed for TESTING, not perfect hardware simulation:
/// - Captures all transmitted bytes for analysis
/// - Expands bytes to 10-bit waveforms (start + 8 data + stop)
/// - Validates UART framing correctness (start bit LOW, stop bit HIGH)
/// - Simulates transmission timing (configurable delay)
/// - Exposes internal state for assertions
///
/// ## Usage in Unit Tests
///
/// ```cpp
/// // Create mock peripheral instance
/// UartPeripheralMock mock;
///
/// // Configure
/// UartConfig config = {
///     .mBaudRate = 3200000,
///     .mTxPin = 17,
///     .mRxPin = -1,
///     .mTxBufferSize = 4096,
///     .mRxBufferSize = 0,
///     .mStopBits = 1,
///     .mUartNum = 1
/// };
/// mock.initialize(config);
///
/// // Transmit data
/// uint8_t data[] = {0xA5, 0x5A};
/// mock.writeBytes(data, 2);
/// mock.waitTxDone(1000);
///
/// // Validate captured data
/// auto captured = mock.getCapturedBytes();
/// REQUIRE(captured.size() == 2);
/// CHECK(captured[0] == 0xA5);
/// CHECK(captured[1] == 0x5A);
///
/// // Validate waveform (includes start/stop bits)
/// auto waveform = mock.getWaveformWithFraming();
/// REQUIRE(waveform.size() == 20);  // 2 bytes × 10 bits
///
/// // Verify start/stop bits
/// CHECK(mock.verifyStartStopBits());
/// ```
///
/// ## Waveform Format (8N1)
///
/// Each captured byte is expanded to a 10-bit frame:
/// ```
/// Byte 0xA5 (0b10100101) becomes:
///   [0] = 0 (START bit - LOW)
///   [1] = 1 (B0 - LSB)
///   [2] = 0 (B1)
///   [3] = 1 (B2)
///   [4] = 0 (B3)
///   [5] = 0 (B4)
///   [6] = 1 (B5)
///   [7] = 0 (B6)
///   [8] = 1 (B7 - MSB)
///   [9] = 1 (STOP bit - HIGH)
/// ```

#pragma once

// Mock implementation has no platform guards - runs on all platforms for testing
#include "platforms/esp/32/drivers/uart/iuart_peripheral.h"
#include "fl/stl/vector.h"
#include "fl/stl/stdint.h"

namespace fl {

//=============================================================================
// Mock Peripheral Implementation
//=============================================================================

/// @brief Mock UART peripheral for unit testing
///
/// Simulates UART hardware with byte capture and waveform extraction.
/// Designed for host-based testing without real ESP32 hardware.
class UartPeripheralMock : public IUartPeripheral {
public:
    //=========================================================================
    // Lifecycle
    //=========================================================================

    UartPeripheralMock();
    ~UartPeripheralMock() override;

    //=========================================================================
    // IUartPeripheral Interface Implementation
    //=========================================================================

    bool initialize(const UartConfig& config) override;
    void deinitialize() override;
    bool isInitialized() const override;
    bool writeBytes(const uint8_t* data, size_t length) override;
    bool waitTxDone(uint32_t timeout_ms) override;
    bool isBusy() const override;
    const UartConfig& getConfig() const override;

    //=========================================================================
    // Mock-Specific API (for unit tests)
    //=========================================================================

    //-------------------------------------------------------------------------
    // Simulation Control
    //-------------------------------------------------------------------------

    /// @brief Set simulated transmission delay
    /// @param microseconds Delay in microseconds (0 = instant)
    ///
    /// Simulates hardware transmission time. Affects waitTxDone() behavior.
    /// Default: 0 (instant transmission for fast tests)
    void setTransmissionDelay(uint32_t microseconds);

    /// @brief Force immediate transmission completion
    ///
    /// Marks all pending transmissions as complete regardless of delay.
    /// Useful for tests that need to skip timing simulation.
    void forceTransmissionComplete();

    /// @brief Reset mock state to initial conditions
    ///
    /// Clears:
    /// - Captured byte history
    /// - Transmission state (mBusy)
    /// - Configuration (mInitialized = false)
    /// - Timing state
    ///
    /// Call between tests to ensure clean state.
    void reset();

    //-------------------------------------------------------------------------
    // Data Capture (for validation)
    //-------------------------------------------------------------------------

    /// @brief Get all captured bytes in transmission order
    /// @return Vector of transmitted bytes (chronological order)
    ///
    /// Returns raw bytes submitted via writeBytes(). Does NOT include
    /// start/stop bits. Use getWaveformWithFraming() for full UART frames.
    fl::vector<uint8_t> getCapturedBytes() const;

    /// @brief Get number of captured bytes
    /// @return Total number of bytes captured since last reset()
    size_t getCapturedByteCount() const;

    /// @brief Clear captured byte history
    ///
    /// Resets captured data without affecting configuration or state.
    /// Useful for multi-phase tests.
    void resetCapturedData();

    /// @brief Get the calculated reset duration for the last transmission
    /// @return Reset duration in microseconds (0 if no transmission occurred)
    ///
    /// Returns the reset duration that the mock calculated for the last
    /// transmission. This is based on transmission time and WS2812 requirements.
    /// Use this to verify reset timing logic without wall-clock measurements.
    uint64_t getLastCalculatedResetDurationUs() const;

    //-------------------------------------------------------------------------
    // Virtual Time Control (for deterministic testing)
    //-------------------------------------------------------------------------

    /// @brief Enable or disable virtual time mode
    /// @param enabled true to use virtual time, false to use real wall-clock time
    ///
    /// When virtual time is enabled, the mock uses an internal virtual clock
    /// instead of real wall-clock time. Time only advances when you call
    /// advanceTime(). This eliminates race conditions in parallel tests.
    void setVirtualTimeMode(bool enabled);

    /// @brief Advance virtual time by specified amount
    /// @param microseconds Amount to advance virtual time
    ///
    /// Only has effect when virtual time mode is enabled.
    /// Use this instead of std::this_thread::sleep_for() for deterministic testing.
    void advanceTime(uint64_t microseconds);

    /// @brief Get current virtual time
    /// @return Current virtual time in microseconds
    ///
    /// Returns 0 if virtual time mode is disabled.
    uint64_t getVirtualTime() const;

    //-------------------------------------------------------------------------
    // Waveform Extraction (for validation)
    //-------------------------------------------------------------------------

    /// @brief Extract full UART waveform including start/stop bits
    /// @return Vector of bits (false = LOW, true = HIGH)
    ///
    /// Expands captured bytes to 10-bit frames (8N1):
    /// - Bit 0: START (always false/LOW)
    /// - Bits 1-8: Data bits (LSB first)
    /// - Bit 9: STOP (always true/HIGH)
    ///
    /// Example:
    /// - 1 byte captured → 10 bits returned
    /// - 3 bytes captured → 30 bits returned
    ///
    /// Use for waveform timing analysis and protocol validation.
    fl::vector<bool> getWaveformWithFraming() const;

    /// @brief Verify start/stop bit correctness for all frames
    /// @return true if all frames valid, false if any frame has incorrect start/stop
    ///
    /// Validates:
    /// - Every frame starts with LOW (start bit)
    /// - Every frame ends with HIGH (stop bit)
    ///
    /// Returns false if:
    /// - Any start bit is HIGH
    /// - Any stop bit is LOW
    /// - No data has been captured yet
    bool verifyStartStopBits() const;

private:
    //=========================================================================
    // Internal State
    //=========================================================================

    UartConfig mConfig;                    ///< Current configuration
    bool mInitialized;                     ///< Initialization state
    bool mBusy;                            ///< Transmission in progress
    fl::vector<uint8_t> mCapturedData;     ///< Captured byte history
    uint32_t mTransmissionDelayUs;         ///< Simulated TX delay (microseconds)
    bool mManualDelaySet;                  ///< True if delay was manually set via setTransmissionDelay()
    uint64_t mLastWriteTimestamp;          ///< Timestamp of last writeBytes() call
    uint64_t mResetExpireTime;             ///< Timestamp when reset period expires
    uint64_t mLastCalculatedResetDuration; ///< Calculated reset duration (for testing)
    bool mVirtualTimeEnabled;              ///< True if using virtual time instead of wall-clock
    uint64_t mVirtualTime;                 ///< Current virtual time in microseconds

    //=========================================================================
    // Internal Helpers
    //=========================================================================

    /// @brief Get current simulated timestamp in microseconds
    /// @return Monotonic timestamp (microseconds)
    uint64_t getCurrentTimestamp() const;

    /// @brief Check if transmission is complete based on timing
    /// @return true if enough time has elapsed, false otherwise
    bool isTransmissionComplete() const;
};

} // namespace fl
