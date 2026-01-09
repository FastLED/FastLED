/// @file spi_peripheral_mock.h
/// @brief Mock SPI peripheral for unit testing
///
/// This class simulates ESP32 SPI hardware behavior for host-based unit tests.
/// It provides:
/// - Waveform data capture for validation
/// - Transaction completion callback simulation
/// - Error injection for negative testing
/// - State inspection for debugging
///
/// ## Design Philosophy
///
/// The mock implementation is designed for TESTING, not perfect hardware simulation:
/// - Captures all transmitted data for analysis
/// - Provides hooks to inject failures
/// - Simulates transaction completion timing (simplified model)
/// - Exposes internal state for assertions
///
/// ## Usage in Unit Tests
///
/// ```cpp
/// // Get singleton mock peripheral instance
/// auto& mock = SpiPeripheralMock::instance();
///
/// // Configure bus
/// SpiBusConfig bus_config(/*mosi=*/23, /*sclk=*/18);
/// mock.initializeBus(bus_config);
///
/// // Add device
/// SpiDeviceConfig device_config(/*clock_hz=*/2500000, /*queue_size=*/3);
/// mock.addDevice(device_config);
///
/// // Register callback
/// mock.registerCallback(callback, ctx);
///
/// // Queue transaction
/// SpiTransaction trans(buffer, size);
/// mock.queueTransaction(trans);
///
/// // Simulate transaction complete (trigger callback)
/// mock.simulateTransactionComplete();
///
/// // Inspect captured data
/// const auto& history = mock.getTransactionHistory();
/// REQUIRE(history.size() == 1);
/// CHECK(history[0].length_bits == expected_bits);
/// ```
///
/// ## Singleton Access Pattern
///
/// For tests that need to inspect mock state after SpiEngine hides it:
/// ```cpp
/// SpiEngine& engine = SpiEngine::getInstance();
/// engine.initialize(...);
///
/// // Get mock instance via singleton
/// auto& mock = SpiPeripheralMock::instance();
///
/// // Inspect mock state
/// CHECK(mock.isInitialized());
/// CHECK(mock.getBusConfig().mosi_pin == 23);
/// ```

#pragma once

// Mock implementation has no platform guards - runs on all platforms for testing
#include "platforms/esp/32/drivers/spi/ispi_peripheral.h"
#include "fl/stl/vector.h"
#include "fl/stl/stdint.h"
#include "fl/stl/span.h"

namespace fl {
namespace detail {

//=============================================================================
// Mock Peripheral Interface
//=============================================================================

/// @brief Mock SPI peripheral for unit testing
///
/// Simulates SPI hardware with data capture and transaction simulation.
/// Designed for host-based testing without real ESP32 hardware.
///
/// This is an abstract interface - use instance() to access the singleton.
class SpiPeripheralMock : public ISpiPeripheral {
public:
    //=========================================================================
    // Singleton Access
    //=========================================================================

    /// @brief Get the singleton mock peripheral instance
    /// @return Reference to the singleton mock peripheral
    ///
    /// This mirrors the hardware constraint that there is only one SPI peripheral.
    /// The instance is created on first access and persists for the program lifetime.
    static SpiPeripheralMock& instance();

    //=========================================================================
    // Lifecycle
    //=========================================================================

    ~SpiPeripheralMock() override = default;

    //=========================================================================
    // ISpiPeripheral Interface Implementation
    //=========================================================================

    bool initializeBus(const SpiBusConfig& config) override = 0;
    bool addDevice(const SpiDeviceConfig& config) override = 0;
    bool removeDevice() override = 0;
    bool freeBus() override = 0;
    bool isInitialized() const override = 0;
    bool queueTransaction(const SpiTransaction& trans) override = 0;
    bool pollTransaction(uint32_t timeout_ms) override = 0;
    bool registerCallback(void* callback, void* user_ctx) override = 0;
    uint8_t* allocateDma(size_t size) override = 0;
    void freeDma(uint8_t* buffer) override = 0;
    void delay(uint32_t ms) override = 0;
    uint64_t getMicroseconds() override = 0;

    //=========================================================================
    // Mock-Specific API (for unit tests)
    //=========================================================================

    /// @brief Transaction record (captured waveform data)
    struct TransactionRecord {
        fl::vector<uint8_t> buffer_copy;  ///< Copy of transmitted buffer
        size_t length_bits;                ///< Number of bits transmitted
        uint32_t flags;                    ///< Transaction flags
        void* user;                        ///< User context pointer
        uint64_t timestamp_us;             ///< Simulated timestamp (microseconds)
    };

    //-------------------------------------------------------------------------
    // Simulation Control
    //-------------------------------------------------------------------------

    /// @brief Set simulated transaction delay
    /// @param microseconds Delay in microseconds (0 = instant)
    ///
    /// Simulates hardware transmission time. Affects pollTransaction() behavior.
    virtual void setTransactionDelay(uint32_t microseconds) = 0;

    /// @brief Manually trigger transaction completion (fire callback)
    ///
    /// Simulates the hardware "transaction done" interrupt. Calls the
    /// registered post-transaction callback if one is set.
    ///
    /// Use in tests to advance the simulation:
    /// ```cpp
    /// engine.queueTransaction(...);
    /// mock.simulateTransactionComplete();  // Trigger callback
    /// engine.poll();  // Process completion
    /// ```
    virtual void simulateTransactionComplete() = 0;

    /// @brief Inject transaction failure for negative testing
    /// @param should_fail true = fail next queueTransaction(), false = succeed
    ///
    /// Use to test error handling paths:
    /// ```cpp
    /// mock.setTransactionFailure(true);
    /// bool result = engine.queueTransaction(...);
    /// CHECK_FALSE(result);  // Should fail
    /// ```
    virtual void setTransactionFailure(bool should_fail) = 0;

    //-------------------------------------------------------------------------
    // Waveform Capture (for validation)
    //-------------------------------------------------------------------------

    /// @brief Get history of all transmitted transactions
    /// @return Vector of transaction records (chronological order)
    ///
    /// Each record contains a copy of the transmitted buffer, allowing
    /// tests to validate waveform correctness.
    virtual const fl::vector<TransactionRecord>& getTransactionHistory() const = 0;

    /// @brief Clear transaction history (reset for next test)
    virtual void clearTransactionHistory() = 0;

    /// @brief Get the most recent transaction data
    /// @return Span of the most recent transaction buffer (empty if no transactions)
    ///
    /// Returns the captured data from the most recent queueTransaction() call.
    /// Useful for quick validation without iterating through history.
    virtual fl::span<const uint8_t> getLastTransactionData() const = 0;

    //-------------------------------------------------------------------------
    // State Inspection
    //-------------------------------------------------------------------------

    /// @brief Check if device is added to bus
    virtual bool hasDevice() const = 0;

    /// @brief Check if transaction queue has space
    virtual bool canQueueTransaction() const = 0;

    /// @brief Get number of queued transactions (not yet completed)
    virtual size_t getQueuedTransactionCount() const = 0;

    /// @brief Get total number of queueTransaction() calls
    virtual size_t getTransactionCount() const = 0;

    /// @brief Get current bus configuration
    virtual const SpiBusConfig& getBusConfig() const = 0;

    /// @brief Get current device configuration
    virtual const SpiDeviceConfig& getDeviceConfig() const = 0;

    /// @brief Reset mock to uninitialized state (for testing)
    /// @note Waits for worker thread to finish pending transactions
    /// @note Does NOT stop or restart the worker thread
    virtual void reset() = 0;
};

} // namespace detail
} // namespace fl
