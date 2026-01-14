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
/// // Get singleton mock peripheral instance
/// auto& mock = ParlioPeripheralMock::instance();
///
/// // Configure
/// ParlioPeripheralConfig config = {...};
/// mock.initialize(config);
///
/// // Register callback
/// mock.registerTxDoneCallback(callback, ctx);
///
/// // Transmit
/// mock.enable();
/// mock.transmit(buffer, bits, idle);
///
/// // Simulate transmission complete (trigger ISR)
/// mock.simulateTransmitComplete();
///
/// // Inspect captured waveform
/// const auto& history = mock.getTransmissionHistory();
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
/// // Get mock instance via singleton
/// auto& mock = ParlioPeripheralMock::instance();
///
/// // Inspect mock state
/// CHECK(mock.isInitialized());
/// CHECK(mock.getConfig().data_width == 4);
/// ```

#pragma once

// Mock implementation has no platform guards - runs on all platforms for testing
#include "platforms/esp/32/drivers/parlio/iparlio_peripheral.h"
#include "fl/stl/vector.h"
#include "fl/stl/stdint.h"
#include "fl/stl/span.h"
#include "fl/stl/map.h"

namespace fl {
namespace detail {

//=============================================================================
// Mock Peripheral Interface
//=============================================================================

/// @brief Mock PARLIO peripheral for unit testing
///
/// Simulates PARLIO hardware with data capture and ISR simulation.
/// Designed for host-based testing without real ESP32 hardware.
///
/// This is an abstract interface - use instance() to access the singleton.
class ParlioPeripheralMock : public IParlioPeripheral {
public:
    //=========================================================================
    // Singleton Access
    //=========================================================================

    /// @brief Get the singleton mock peripheral instance
    /// @return Reference to the singleton mock peripheral
    ///
    /// This mirrors the hardware constraint that there is only one PARLIO peripheral.
    /// The instance is created on first access and persists for the program lifetime.
    static ParlioPeripheralMock& instance();

    //=========================================================================
    // Lifecycle
    //=========================================================================

    ~ParlioPeripheralMock() override = default;

    //=========================================================================
    // IParlioPeripheral Interface Implementation
    //=========================================================================

    bool initialize(const ParlioPeripheralConfig& config) override = 0;
    bool enable() override = 0;
    bool disable() override = 0;
    bool transmit(const uint8_t* buffer, size_t bit_count, uint16_t idle_value) override = 0;
    bool waitAllDone(uint32_t timeout_ms) override = 0;
    bool registerTxDoneCallback(void* callback, void* user_ctx) override = 0;
    uint8_t* allocateDmaBuffer(size_t size) override = 0;
    void freeDmaBuffer(uint8_t* buffer) override = 0;
    void delay(uint32_t ms) override = 0;
    uint64_t getMicroseconds() override = 0;
    void freeDmaBuffer(void* ptr) override = 0;

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
    virtual void setTransmitDelay(uint32_t microseconds) = 0;

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
    virtual void simulateTransmitComplete() = 0;

    /// @brief Inject transmission failure for negative testing
    /// @param should_fail true = fail next transmit(), false = succeed
    ///
    /// Use to test error handling paths:
    /// ```cpp
    /// mock->setTransmitFailure(true);
    /// bool result = engine.beginTransmission(...);
    /// CHECK_FALSE(result);  // Should fail
    /// ```
    virtual void setTransmitFailure(bool should_fail) = 0;

    //-------------------------------------------------------------------------
    // Waveform Capture (for validation)
    //-------------------------------------------------------------------------

    /// @brief Get history of all transmitted waveforms
    /// @return Vector of transmission records (chronological order)
    ///
    /// Each record contains a copy of the transmitted buffer, allowing
    /// tests to validate waveform correctness.
    virtual const fl::vector<TransmissionRecord>& getTransmissionHistory() const = 0;

    /// @brief Clear transmission history (reset for next test)
    virtual void clearTransmissionHistory() = 0;

    /// @brief Get transmission data for a specific GPIO pin from the most recent transmission
    /// @param gpio_pin GPIO pin number (e.g., 1, 2, 4, 8 - actual pin numbers from config)
    /// @return Span of transmission data for the specified pin (empty if no transmission or invalid pin)
    ///
    /// Returns the untransposed waveform data for a specific GPIO pin from the most recent
    /// transmission. This is useful for validating per-pin output patterns in tests.
    ///
    /// Example:
    /// ```cpp
    /// auto& mock = ParlioPeripheralMock::instance();
    /// // ... initialize with pins {1, 2} and perform transmission ...
    /// fl::span<const uint8_t> pin1_data = mock.getTransmissionDataForPin(1);
    /// REQUIRE(pin1_data[0] == 0xFF);  // Check first byte of GPIO pin 1 waveform
    /// ```
    virtual fl::span<const uint8_t> getTransmissionDataForPin(int gpio_pin) const = 0;

    /// @brief Untranspose interleaved bit-parallel data to per-pin waveforms
    /// @param transposed_data Interleaved bit data (output from wave8Transpose_N)
    /// @param pins GPIO pin numbers corresponding to lanes in the transposed data
    /// @return Map of GPIO pin number to waveform data
    ///
    /// This function reverses the transposition performed by wave8Transpose_N to extract
    /// the original waveform for each pin. The transposed data is in bit-parallel format
    /// where bits from multiple pins are interleaved.
    ///
    /// Example for 2-lane:
    /// ```cpp
    /// fl::vector_fixed<int, 2> pins = {1, 2};
    /// fl::vector<uint8_t> transposed = {0xAA, 0xAA, ...};  // Alternating bits
    /// auto result = untransposeParlioBitstream(transposed, pins);
    /// // result[1] = {0xFF, 0xFF, ...}  // Lane 0 waveform (all high)
    /// // result[2] = {0x00, 0x00, ...}  // Lane 1 waveform (all low)
    /// ```
    static fl::vector<fl::pair<int, fl::vector<uint8_t>>> untransposeParlioBitstream(
        fl::span<const uint8_t> transposed_data,
        fl::span<const int> pins,  // Pin order that will be return in the result.
        ParlioBitPackOrder packing = ParlioBitPackOrder::FL_PARLIO_MSB);

    //-------------------------------------------------------------------------
    // State Inspection
    //-------------------------------------------------------------------------

    /// @brief Check if peripheral is initialized
    bool isInitialized() const override = 0;

    /// @brief Check if peripheral is enabled
    virtual bool isEnabled() const = 0;

    /// @brief Check if transmission is in progress
    virtual bool isTransmitting() const = 0;

    /// @brief Get total number of transmit() calls
    /// @note Waits for all pending transmissions to complete before returning
    virtual size_t getTransmitCount() const = 0;

    /// @brief Get current configuration
    virtual const ParlioPeripheralConfig& getConfig() const = 0;

    /// @brief Reset mock to uninitialized state (for testing)
    /// @note Waits for worker thread to finish pending transmissions
    /// @note Does NOT stop or restart the worker thread
    virtual void reset() = 0;
};

//=============================================================================
// Singleton Access for Tests
//=============================================================================


} // namespace detail
} // namespace fl
