/// @file i2s_lcd_cam_peripheral_mock.h
/// @brief Mock I2S LCD_CAM peripheral for unit testing
///
/// This class simulates ESP32-S3 I2S LCD_CAM hardware behavior for host-based unit tests.
/// It provides:
/// - Frame data capture for validation
/// - ISR callback simulation
/// - Error injection for negative testing
/// - State inspection for debugging
///
/// ## Design Philosophy
///
/// The mock implementation captures the minimal low-level operations to enable
/// maximum unit test coverage of the driver logic without real hardware.
///
/// ## Usage in Unit Tests
///
/// ```cpp
/// // Get singleton mock peripheral instance
/// auto& mock = I2sLcdCamPeripheralMock::instance();
/// mock.reset();
///
/// // Configure
/// I2sLcdCamConfig config = {...};
/// mock.initialize(config);
///
/// // Register callback
/// mock.registerTransmitCallback(callback, ctx);
///
/// // Transmit
/// uint16_t* buffer = mock.allocateBuffer(size);
/// // ... fill buffer ...
/// mock.transmit(buffer, size);
///
/// // Simulate completion
/// mock.simulateTransmitComplete();
///
/// // Inspect captured data
/// const auto& history = mock.getTransmitHistory();
/// REQUIRE(history.size() == 1);
/// CHECK(history[0].size_bytes == expected_size);
/// ```

#pragma once

// Mock implementation has no platform guards - runs on all platforms for testing
#include "platforms/esp/32/drivers/i2s/ii2s_lcd_cam_peripheral.h"
#include "fl/stl/vector.h"
#include "fl/stl/stdint.h"
#include "fl/stl/span.h"

namespace fl {
namespace detail {

//=============================================================================
// Mock Peripheral Interface
//=============================================================================

/// @brief Mock I2S LCD_CAM peripheral for unit testing
///
/// Simulates I2S LCD_CAM hardware with data capture and ISR simulation.
/// Designed for host-based testing without real ESP32-S3 hardware.
class I2sLcdCamPeripheralMock : public II2sLcdCamPeripheral {
public:
    //=========================================================================
    // Singleton Access
    //=========================================================================

    /// @brief Get the singleton mock peripheral instance
    /// @return Reference to the singleton mock peripheral
    ///
    /// This mirrors the hardware constraint that there is only one I2S LCD_CAM peripheral.
    static I2sLcdCamPeripheralMock& instance();

    //=========================================================================
    // Lifecycle
    //=========================================================================

    ~I2sLcdCamPeripheralMock() override = default;

    //=========================================================================
    // II2sLcdCamPeripheral Interface Implementation
    //=========================================================================

    bool initialize(const I2sLcdCamConfig& config) override = 0;
    void deinitialize() override = 0;
    bool isInitialized() const override = 0;

    uint16_t* allocateBuffer(size_t size_bytes) override = 0;
    void freeBuffer(uint16_t* buffer) override = 0;

    bool transmit(const uint16_t* buffer, size_t size_bytes) override = 0;
    bool waitTransmitDone(uint32_t timeout_ms) override = 0;
    bool isBusy() const override = 0;

    bool registerTransmitCallback(void* callback, void* user_ctx) override = 0;
    const I2sLcdCamConfig& getConfig() const override = 0;

    uint64_t getMicroseconds() override = 0;
    void delay(uint32_t ms) override = 0;

    //=========================================================================
    // Mock-Specific API (for unit tests)
    //=========================================================================

    /// @brief Transmit record (captured data)
    struct TransmitRecord {
        fl::vector<uint16_t> buffer_copy;  ///< Copy of transmitted buffer
        size_t size_bytes;                  ///< Size in bytes
        uint64_t timestamp_us;              ///< Capture timestamp
    };

    //-------------------------------------------------------------------------
    // Simulation Control
    //-------------------------------------------------------------------------

    /// @brief Manually trigger transmit completion (fire ISR callback)
    ///
    /// Simulates the hardware "transmit complete" interrupt. Calls the
    /// registered callback if one is set.
    virtual void simulateTransmitComplete() = 0;

    /// @brief Inject transmit failure for negative testing
    /// @param should_fail true = fail next transmit(), false = succeed
    virtual void setTransmitFailure(bool should_fail) = 0;

    /// @brief Set simulated transmit delay
    /// @param microseconds Delay in microseconds (0 = instant)
    virtual void setTransmitDelay(uint32_t microseconds) = 0;

    //-------------------------------------------------------------------------
    // Data Capture (for validation)
    //-------------------------------------------------------------------------

    /// @brief Get history of all transmissions
    /// @return Vector of transmit records (chronological order)
    virtual const fl::vector<TransmitRecord>& getTransmitHistory() const = 0;

    /// @brief Clear transmit history (reset for next test)
    virtual void clearTransmitHistory() = 0;

    /// @brief Get most recent transmitted data as span
    /// @return Span of uint16_t data (empty if no transmissions)
    virtual fl::span<const uint16_t> getLastTransmitData() const = 0;

    //-------------------------------------------------------------------------
    // State Inspection
    //-------------------------------------------------------------------------

    /// @brief Check if peripheral is enabled
    virtual bool isEnabled() const = 0;

    /// @brief Get total number of transmit() calls
    virtual size_t getTransmitCount() const = 0;

    /// @brief Reset mock to uninitialized state
    virtual void reset() = 0;
};

} // namespace detail
} // namespace fl
