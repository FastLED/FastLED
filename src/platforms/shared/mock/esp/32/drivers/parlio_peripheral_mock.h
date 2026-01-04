/// @file parlio_peripheral_mock.h
/// @brief Mock PARLIO peripheral for unit testing
///
/// This class simulates ESP32 PARLIO hardware behavior for host-based unit tests.
/// It provides:
/// - Waveform data capture for validation
/// - ISR callback simulation
/// - Error injection for negative testing
/// - State inspection for debugging
///
/// ## Design Philosophy
///
/// The mock implementation is designed for TESTING, not perfect hardware simulation:
/// - Captures all transmitted data for analysis
/// - Provides hooks to inject failures
/// - Simulates ISR timing (simplified model)
/// - Exposes internal state for assertions
///
/// ## Usage in Unit Tests
///
/// ```cpp
/// // Create mock peripheral
/// auto mock = new ParlioPeripheralMock();
///
/// // Configure
/// ParlioPeripheralConfig config = {...};
/// mock->initialize(config);
///
/// // Register callback
/// mock->registerTxDoneCallback(callback, ctx);
///
/// // Transmit
/// mock->enable();
/// mock->transmit(buffer, bits, idle);
///
/// // Simulate transmission complete (trigger ISR)
/// mock->simulateTransmitComplete();
///
/// // Inspect captured waveform
/// const auto& history = mock->getTransmissionHistory();
/// REQUIRE(history.size() == 1);
/// CHECK(history[0].bit_count == expected_bits);
/// ```
///
/// ## Singleton Access Pattern
///
/// For tests that need to inspect mock state after ParlioEngine hides it:
/// ```cpp
/// ParlioEngine& engine = ParlioEngine::getInstance();
/// engine.initialize(...);
///
/// // Get mock instance via singleton accessor
/// auto* mock = getParlioMockInstance();
/// REQUIRE(mock != nullptr);
///
/// // Inspect mock state
/// CHECK(mock->isInitialized());
/// CHECK(mock->getConfig().data_width == 4);
/// ```

#pragma once

// Mock implementation has no platform guards - runs on all platforms for testing
#include "platforms/esp/32/drivers/parlio/iparlio_peripheral.h"
#include "fl/stl/vector.h"
#include "fl/stl/stdint.h"

namespace fl {
namespace detail {

//=============================================================================
// Mock Peripheral Implementation
//=============================================================================

/// @brief Mock PARLIO peripheral for unit testing
///
/// Simulates PARLIO hardware with data capture and ISR simulation.
/// Designed for host-based testing without real ESP32 hardware.
class ParlioPeripheralMock : public IParlioPeripheral {
public:
    //=========================================================================
    // Lifecycle
    //=========================================================================

    ParlioPeripheralMock();
    ~ParlioPeripheralMock() override = default;

    //=========================================================================
    // IParlioPeripheral Interface Implementation
    //=========================================================================

    bool initialize(const ParlioPeripheralConfig& config) override;
    bool enable() override;
    bool disable() override;
    bool transmit(const uint8_t* buffer, size_t bit_count, uint16_t idle_value) override;
    bool waitAllDone(uint32_t timeout_ms) override;
    bool registerTxDoneCallback(void* callback, void* user_ctx) override;
    uint8_t* allocateDmaBuffer(size_t size) override;
    void freeDmaBuffer(uint8_t* buffer) override;
    void delay(uint32_t ms) override;

    //=========================================================================
    // Mock-Specific API (for unit tests)
    //=========================================================================

    /// @brief Transmission record (captured waveform data)
    struct TransmissionRecord {
        fl::vector<uint8_t> buffer_copy;  ///< Copy of transmitted buffer
        size_t bit_count;                  ///< Number of bits transmitted
        uint16_t idle_value;               ///< Idle value used
        uint64_t timestamp_us;             ///< Simulated timestamp (microseconds)
    };

    //-------------------------------------------------------------------------
    // Simulation Control
    //-------------------------------------------------------------------------

    /// @brief Set simulated transmission delay
    /// @param microseconds Delay in microseconds (0 = instant)
    ///
    /// Simulates hardware transmission time. Affects waitAllDone() behavior.
    void setTransmitDelay(uint32_t microseconds);

    /// @brief Manually trigger transmission completion (fire ISR callback)
    ///
    /// Simulates the hardware "transmission done" interrupt. Calls the
    /// registered ISR callback if one is set.
    ///
    /// Use in tests to advance the simulation:
    /// ```cpp
    /// engine.beginTransmission(...);
    /// mock->simulateTransmitComplete();  // Trigger ISR
    /// engine.poll();  // Process completion
    /// ```
    void simulateTransmitComplete();

    /// @brief Inject transmission failure for negative testing
    /// @param should_fail true = fail next transmit(), false = succeed
    ///
    /// Use to test error handling paths:
    /// ```cpp
    /// mock->setTransmitFailure(true);
    /// bool result = engine.beginTransmission(...);
    /// CHECK_FALSE(result);  // Should fail
    /// ```
    void setTransmitFailure(bool should_fail);

    //-------------------------------------------------------------------------
    // Waveform Capture (for validation)
    //-------------------------------------------------------------------------

    /// @brief Get history of all transmitted waveforms
    /// @return Vector of transmission records (chronological order)
    ///
    /// Each record contains a copy of the transmitted buffer, allowing
    /// tests to validate waveform correctness.
    const fl::vector<TransmissionRecord>& getTransmissionHistory() const;

    /// @brief Clear transmission history (reset for next test)
    void clearTransmissionHistory();

    //-------------------------------------------------------------------------
    // State Inspection
    //-------------------------------------------------------------------------

    /// @brief Check if peripheral is initialized
    bool isInitialized() const { return mInitialized; }

    /// @brief Check if peripheral is enabled
    bool isEnabled() const { return mEnabled; }

    /// @brief Check if transmission is in progress
    bool isTransmitting() const { return mTransmitting; }

    /// @brief Get total number of transmit() calls
    size_t getTransmitCount() const { return mTransmitCount; }

    /// @brief Get current configuration
    const ParlioPeripheralConfig& getConfig() const { return mConfig; }

private:
    //=========================================================================
    // Internal State
    //=========================================================================

    // Lifecycle state
    bool mInitialized;
    bool mEnabled;
    bool mTransmitting;
    size_t mTransmitCount;
    ParlioPeripheralConfig mConfig;

    // ISR callback
    void* mCallback;
    void* mUserCtx;

    // Simulation settings
    uint32_t mTransmitDelayUs;
    bool mShouldFailTransmit;

    // Waveform capture
    fl::vector<TransmissionRecord> mHistory;

    // Pending transmission state (for waitAllDone simulation)
    size_t mPendingTransmissions;
};

//=============================================================================
// Singleton Access for Tests
//=============================================================================

/// @brief Get the global mock instance (for unit tests)
/// @return Pointer to mock instance, or nullptr if not using mock
///
/// This function allows unit tests to access the mock peripheral even after
/// ParlioEngine has encapsulated it. Use ONLY in test code, not production.
///
/// Example:
/// ```cpp
/// TEST_CASE("ParlioEngine waveform validation") {
///     ParlioEngine& engine = ParlioEngine::getInstance();
///     engine.initialize(...);
///     engine.beginTransmission(...);
///
///     auto* mock = getParlioMockInstance();
///     REQUIRE(mock != nullptr);
///     const auto& history = mock->getTransmissionHistory();
///     CHECK(history[0].bit_count == expected);
/// }
/// ```
ParlioPeripheralMock* getParlioMockInstance();

} // namespace detail
} // namespace fl
