/// @file rmt5_peripheral_mock.h
/// @brief Mock RMT5 peripheral for unit testing
///
/// This class simulates ESP32 RMT5 hardware behavior for host-based unit tests.
/// It provides:
/// - Waveform data capture for validation
/// - Transmission completion callback simulation
/// - Error injection for negative testing
/// - State inspection for debugging
///
/// ## Design Philosophy
///
/// The mock implementation is designed for TESTING, not perfect hardware simulation:
/// - Captures all transmitted pixel data for analysis
/// - Provides hooks to inject failures
/// - Simulates transmission completion timing (simplified model)
/// - Exposes internal state for assertions
///
/// ## Usage in Unit Tests
///
/// ```cpp
/// // Get singleton mock peripheral instance
/// auto& mock = Rmt5PeripheralMock::instance();
///
/// // Create channel
/// void* channel_handle;
/// Rmt5ChannelConfig config(18, 40000000, 64, 1, true);
/// mock.createTxChannel(config, &channel_handle);
///
/// // Create encoder
/// void* encoder = mock.createEncoder(WS2812_TIMING, 40000000);
///
/// // Register callback
/// mock.registerTxCallback(channel_handle, callback, ctx);
///
/// // Transmit pixels
/// uint8_t pixels[] = {0xFF, 0x00, 0x00};  // Red pixel
/// mock.transmit(channel_handle, encoder, pixels, sizeof(pixels));
///
/// // Simulate transmission complete (trigger callback)
/// mock.simulateTransmitDone(channel_handle);
///
/// // Inspect captured data
/// const auto& history = mock.getTransmissionHistory();
/// REQUIRE(history.size() == 1);
/// CHECK(verifyPixelData(history[0], {0xFF, 0x00, 0x00}));
/// ```
///
/// ## Singleton Access Pattern
///
/// For tests that need to inspect mock state after ChannelEngineRMT hides it:
/// ```cpp
/// auto engine = ChannelEngineRMT::create();
/// engine->initialize(...);
///
/// // Get mock instance via singleton
/// auto& mock = Rmt5PeripheralMock::instance();
///
/// // Inspect mock state
/// CHECK(mock.getChannelCount() > 0);
/// ```

#pragma once

// Mock implementation has no platform guards - runs on all platforms for testing
#include "platforms/esp/32/drivers/rmt/rmt_5/irmt5_peripheral.h"
#include "fl/chipsets/led_timing.h"
#include "fl/stl/vector.h"
#include "fl/stl/stdint.h"
#include "fl/stl/span.h"

namespace fl {
namespace detail {

//=============================================================================
// Mock Peripheral Interface
//=============================================================================

/// @brief Mock RMT5 peripheral for unit testing
///
/// Simulates RMT5 hardware with pixel data capture and transmission simulation.
/// Designed for host-based testing without real ESP32 hardware.
///
/// This is an abstract interface - use instance() to access the singleton.
class Rmt5PeripheralMock : public IRMT5Peripheral {
public:
    //=========================================================================
    // Singleton Access
    //=========================================================================

    /// @brief Get the singleton mock peripheral instance
    /// @return Reference to the singleton mock peripheral
    ///
    /// This mirrors the hardware constraint that there is only one RMT peripheral.
    /// The instance is created on first access and persists for the program lifetime.
    static Rmt5PeripheralMock& instance();

    //=========================================================================
    // Lifecycle
    //=========================================================================

    ~Rmt5PeripheralMock() override = default;

    //=========================================================================
    // IRMT5Peripheral Interface Implementation
    //=========================================================================

    bool createTxChannel(const Rmt5ChannelConfig& config,
                         void** out_handle) override = 0;
    bool deleteChannel(void* channel_handle) override = 0;
    bool enableChannel(void* channel_handle) override = 0;
    bool disableChannel(void* channel_handle) override = 0;
    bool transmit(void* channel_handle, void* encoder_handle,
                  const u8* buffer, size_t buffer_size) override = 0;
    bool waitAllDone(void* channel_handle, u32 timeout_ms) override = 0;
    void* createEncoder(const ChipsetTiming& timing,
                        u32 resolution_hz) override = 0;
    void deleteEncoder(void* encoder_handle) override = 0;
    bool resetEncoder(void* encoder_handle) override = 0;
    bool registerTxCallback(void* channel_handle,
                            Rmt5TxDoneCallback callback,
                            void* user_ctx) override = 0;
    void configureLogging() override = 0;
    bool syncCache(void* buffer, size_t size) override = 0;
    u8* allocateDmaBuffer(size_t size) override = 0;
    void freeDmaBuffer(u8* buffer) override = 0;

    //=========================================================================
    // Mock-Specific API (for unit tests)
    //=========================================================================

    /// @brief Transmission record (captured pixel data)
    struct TransmissionRecord {
        fl::vector<u8> buffer_copy;  ///< Copy of transmitted pixel data
        size_t buffer_size;                ///< Size of buffer in bytes
        int gpio_pin;                      ///< GPIO pin number
        ChipsetTiming timing;              ///< LED chipset timing
        u32 resolution_hz;            ///< Channel clock resolution
        bool used_dma;                     ///< Whether DMA was enabled
        u64 timestamp_us;             ///< Simulated timestamp (microseconds)
    };

    //-------------------------------------------------------------------------
    // Simulation Control
    //-------------------------------------------------------------------------

    /// @brief Manually trigger transmission completion for a channel (fire callback)
    /// @param channel_handle Channel handle from createTxChannel()
    ///
    /// Simulates the hardware "transmission done" interrupt. Calls the
    /// registered TX callback if one is set for this channel.
    ///
    /// Use in tests to advance the simulation:
    /// ```cpp
    /// engine.transmit(channel, encoder, pixels, size);
    /// mock.simulateTransmitDone(channel);  // Trigger callback
    /// engine.poll();  // Process completion
    /// ```
    virtual void simulateTransmitDone(void* channel_handle) = 0;

    /// @brief Inject transmission failure for negative testing
    /// @param should_fail true = fail next transmit(), false = succeed
    ///
    /// Use to test error handling paths:
    /// ```cpp
    /// mock.setTransmitFailure(true);
    /// bool result = engine.transmit(...);
    /// CHECK_FALSE(result);  // Should fail
    /// ```
    virtual void setTransmitFailure(bool should_fail) = 0;

    //-------------------------------------------------------------------------
    // Waveform Capture (for validation)
    //-------------------------------------------------------------------------

    /// @brief Get history of all transmitted pixel data
    /// @return Vector of transmission records (chronological order)
    ///
    /// Each record contains a copy of the transmitted pixel buffer, allowing
    /// tests to validate pixel data correctness.
    virtual const fl::vector<TransmissionRecord>& getTransmissionHistory() const = 0;

    /// @brief Clear transmission history (reset for next test)
    virtual void clearTransmissionHistory() = 0;

    /// @brief Get the most recent transmission data
    /// @return Span of the most recent pixel buffer (empty if no transmissions)
    ///
    /// Returns the captured data from the most recent transmit() call.
    /// Useful for quick validation without iterating through history.
    virtual fl::span<const u8> getLastTransmissionData() const = 0;

    //-------------------------------------------------------------------------
    // State Inspection
    //-------------------------------------------------------------------------

    /// @brief Get number of active channels
    virtual size_t getChannelCount() const = 0;

    /// @brief Get number of active encoders
    virtual size_t getEncoderCount() const = 0;

    /// @brief Get total number of transmit() calls
    virtual size_t getTransmissionCount() const = 0;

    /// @brief Check if a channel is enabled
    /// @param channel_handle Channel handle from createTxChannel()
    virtual bool isChannelEnabled(void* channel_handle) const = 0;

    /// @brief Reset mock to initial state (for testing)
    ///
    /// This method:
    /// - Deletes all channels and encoders
    /// - Clears transmission history
    /// - Resets error injection flags
    /// - Resets internal ID counters
    virtual void reset() = 0;
};

//=============================================================================
// Validation Helpers
//=============================================================================

/// @brief Verify that transmitted pixel data matches expected data
/// @param record Transmission record from mock
/// @param expected Expected pixel data
/// @return true if data matches, false otherwise
///
/// Simple byte-wise comparison of pixel data.
inline bool verifyPixelData(const Rmt5PeripheralMock::TransmissionRecord& record,
                             fl::span<const u8> expected) {
    if (record.buffer_copy.size() != expected.size()) {
        return false;
    }
    for (size_t i = 0; i < expected.size(); i++) {
        if (record.buffer_copy[i] != expected[i]) {
            return false;
        }
    }
    return true;
}

} // namespace detail
} // namespace fl
