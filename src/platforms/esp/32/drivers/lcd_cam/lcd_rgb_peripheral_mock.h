/// @file lcd_rgb_peripheral_mock.h
/// @brief Mock LCD RGB peripheral for unit testing
///
/// This class simulates ESP32-P4 LCD RGB hardware behavior for host-based unit tests.
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
/// auto& mock = LcdRgbPeripheralMock::instance();
/// mock.reset();
///
/// // Configure
/// LcdRgbPeripheralConfig config = {...};
/// mock.initialize(config);
///
/// // Register callback
/// mock.registerDrawCallback(callback, ctx);
///
/// // Draw frame
/// uint16_t* buffer = mock.allocateFrameBuffer(size);
/// // ... fill buffer ...
/// mock.drawFrame(buffer, size);
///
/// // Simulate completion
/// mock.simulateDrawComplete();
///
/// // Inspect captured frame data
/// const auto& history = mock.getFrameHistory();
/// REQUIRE(history.size() == 1);
/// CHECK(history[0].size_bytes == expected_size);
/// ```

#pragma once

// IWYU pragma: private

// Mock implementation has no platform guards - runs on all platforms for testing
#include "platforms/esp/32/drivers/lcd_cam/ilcd_rgb_peripheral.h"
#include "fl/stl/vector.h"
#include "fl/stl/stdint.h"
#include "fl/stl/span.h"
#include "fl/stl/noexcept.h"

namespace fl {
namespace detail {

//=============================================================================
// Mock Peripheral Interface
//=============================================================================

/// @brief Mock LCD RGB peripheral for unit testing
///
/// Simulates LCD RGB hardware with data capture and ISR simulation.
/// Designed for host-based testing without real ESP32-P4 hardware.
class LcdRgbPeripheralMock : public ILcdRgbPeripheral {
public:
    //=========================================================================
    // Singleton Access
    //=========================================================================

    /// @brief Get the singleton mock peripheral instance
    /// @return Reference to the singleton mock peripheral
    ///
    /// This mirrors the hardware constraint that there is only one LCD RGB peripheral.
    static LcdRgbPeripheralMock& instance() FL_NOEXCEPT;

    //=========================================================================
    // Lifecycle
    //=========================================================================

    ~LcdRgbPeripheralMock() override = default;

    //=========================================================================
    // ILcdRgbPeripheral Interface Implementation
    //=========================================================================

    bool initialize(const LcdRgbPeripheralConfig& config) FL_NOEXCEPT override = 0;
    void deinitialize() FL_NOEXCEPT override = 0;
    bool isInitialized() const FL_NOEXCEPT override = 0;

    u16* allocateFrameBuffer(size_t size_bytes) FL_NOEXCEPT override = 0;
    void freeFrameBuffer(u16* buffer) FL_NOEXCEPT override = 0;

    bool drawFrame(const u16* buffer, size_t size_bytes) FL_NOEXCEPT override = 0;
    bool waitFrameDone(u32 timeout_ms) FL_NOEXCEPT override = 0;
    bool isBusy() const FL_NOEXCEPT override = 0;

    bool registerDrawCallback(void* callback, void* user_ctx) FL_NOEXCEPT override = 0;
    const LcdRgbPeripheralConfig& getConfig() const FL_NOEXCEPT override = 0;

    u64 getMicroseconds() FL_NOEXCEPT override = 0;
    void delay(u32 ms) FL_NOEXCEPT override = 0;

    //=========================================================================
    // Mock-Specific API (for unit tests)
    //=========================================================================

    /// @brief Frame record (captured data)
    struct FrameRecord {
        fl::vector<u16> buffer_copy;  ///< Copy of frame buffer
        size_t size_bytes;                  ///< Size in bytes
        u64 timestamp_us;              ///< Capture timestamp
    };

    //-------------------------------------------------------------------------
    // Simulation Control
    //-------------------------------------------------------------------------

    /// @brief Manually trigger draw completion (fire ISR callback)
    ///
    /// Simulates the hardware "frame complete" interrupt. Calls the
    /// registered callback if one is set.
    virtual void simulateDrawComplete() FL_NOEXCEPT = 0;

    /// @brief Inject draw failure for negative testing
    /// @param should_fail true = fail next drawFrame(), false = succeed
    virtual void setDrawFailure(bool should_fail) FL_NOEXCEPT = 0;

    /// @brief Set simulated draw delay
    /// @param microseconds Delay in microseconds (0 = instant)
    virtual void setDrawDelay(u32 microseconds) FL_NOEXCEPT = 0;

    //-------------------------------------------------------------------------
    // Data Capture (for validation)
    //-------------------------------------------------------------------------

    /// @brief Get history of all drawn frames
    /// @return Vector of frame records (chronological order)
    virtual const fl::vector<FrameRecord>& getFrameHistory() const FL_NOEXCEPT = 0;

    /// @brief Clear frame history (reset for next test)
    virtual void clearFrameHistory() FL_NOEXCEPT = 0;

    /// @brief Get most recent frame data as span
    /// @return Span of uint16_t frame data (empty if no frames)
    virtual fl::span<const u16> getLastFrameData() const FL_NOEXCEPT = 0;

    //-------------------------------------------------------------------------
    // State Inspection
    //-------------------------------------------------------------------------

    /// @brief Check if peripheral is enabled
    virtual bool isEnabled() const FL_NOEXCEPT = 0;

    /// @brief Get total number of drawFrame() calls
    virtual size_t getDrawCount() const FL_NOEXCEPT = 0;

    /// @brief Reset mock to uninitialized state
    virtual void reset() FL_NOEXCEPT = 0;
};

} // namespace detail
} // namespace fl
