#pragma once

/// @file spi.h
/// Multi-lane SPI interface for LED output
/// Hardware (SPI_HW): 1-8 parallel data lanes
/// Software (SPI_BITBANG, SPI_ISR): up to 32 parallel data lanes
/// See examples/Spi/Spi.ino for usage example


#include "fl/stl/stdint.h"
#include "fl/stl/unique_ptr.h"
#include "fl/stl/span.h"

// SPI components
#include "fl/spi/config.h"
#include "fl/spi/transaction.h"
#include "fl/spi/device.h"
#include "fl/spi/lane.h"
#include "fl/spi/write_result.h"  // Defines WriteResult with wait() method
#include "fl/spi/multi_lane_device.h"
#include "platforms/shared/spi_types.h"  // ok platform headers

namespace fl {

// ============================================================================
// Public API - Main SPI Device Class
// ============================================================================

/// @brief SPI Device - RAII wrapper for multi-lane SPI
/// @details This is the main user-facing class. It wraps device creation,
///          initialization, and provides a simple write() API.
///          Automatically creates and initializes the device on construction.
///
///          **Lane Support:**
///          - Hardware (SPI_HW): 1-8 lanes (platform-dependent, uses DMA)
///          - Software (SPI_BITBANG): up to 32 lanes (CPU bit-banging, blocking)
///          - Software (SPI_ISR): up to 32 lanes (ISR-driven, async)
///
/// **Example:**
/// @code
/// int pins[] = {0, 1, 2, 5};
/// fl::Spi spi(CLOCK_PIN, pins, fl::SPI_HW);
/// if (!spi.ok()) {
///     FL_WARN("SPI initialization failed: " << spi.error());
///     return;
/// }
/// spi.write(lane0, lane1, lane2, lane3);
/// spi.wait();  // Block until transmission completes
/// @endcode
class Spi {
public:
    /// @brief Default constructor - creates device in error state
    Spi() : is_ok(false), error_code(SPIError::NOT_INITIALIZED) {}

    /// @brief Construct and initialize SPI device
    /// @param clock_pin Clock pin number
    /// @param data_pins Data pin numbers (1-8 for SPI_HW, up to 32 for software modes)
    /// @param output_mode Output mode (SPI_HW for hardware, SPI_BITBANG/SPI_ISR for software)
    /// @param clock_speed_hz Clock speed in Hz (0xffffffff = as fast as possible)
    Spi(int clock_pin, fl::span<const int> data_pins,
        spi_output_mode_t output_mode = SPI_HW,
        uint32_t clock_speed_hz = 0xffffffff);

    /// @brief Construct from SpiConfig
    /// @param config SPI configuration
    explicit Spi(const SpiConfig& config);

    /// @brief Move constructor
    Spi(Spi&& other) noexcept;

    /// @brief Move assignment
    Spi& operator=(Spi&& other) noexcept;

    /// @brief Check if device was created and initialized successfully
    bool ok() const { return is_ok; }

    /// @brief Get error code (only meaningful if !ok())
    SPIError error() const { return error_code; }

    /// @brief Explicit conversion to bool for contextual evaluation
    explicit operator bool() const { return ok(); }

    /// @brief Write multiple lanes in parallel (variadic template)
    /// @tparam Spans Variadic template parameter pack (all must be convertible to fl::span<const uint8_t>)
    /// @param lanes Lane data as spans (1-8 for hardware, up to 32 for software modes)
    /// @returns WriteResult with ok=true on success, or ok=false with error message
    /// @note Automatically waits for previous transmission, then starts new one **asynchronously**
    /// @note Call wait() to block until transmission completes
    /// @note Internally unpacks to FixedVector for atomic processing (no heap allocation)
    /// @note Hardware modes (SPI_HW) support 1-8 lanes, software modes support up to 32 lanes
    /// @note **IMPORTANT**: All lanes MUST have identical sizes. Operation fails if sizes differ.
    /// @note Users must handle chipset-specific padding at application level before calling write()
    /// @code{.cpp}
    /// // All lanes must have the same size!
    /// fl::array<uint8_t, 100> lane0 = {...};
    /// fl::array<uint8_t, 100> lane1 = {...};  // Same size!
    /// fl::array<uint8_t, 100> lane2 = {...};  // Same size!
    /// fl::array<uint8_t, 100> lane3 = {...};  // Same size!
    ///
    /// // Async usage (fire and forget)
    /// auto result = spi.write(lane0, lane1, lane2, lane3);
    /// if (!result.ok) {
    ///     FL_WARN("Write failed: " << result.error);
    /// }
    /// // ... do other work while transmission happens in background ...
    ///
    /// // Sync usage (wait for completion)
    /// spi.write(lane0, lane1, lane2, lane3);
    /// spi.wait();  // Block until transmission completes
    /// @endcode
    template<typename... Spans>
    WriteResult write(Spans&&... lanes) {
        if (!device) {
            return WriteResult("SPI device not initialized");
        }
        return device->write(fl::forward<Spans>(lanes)...);
    }

    /// @brief Wait for async write operation to complete
    /// @param timeout_ms Maximum time to wait in milliseconds (default: wait forever)
    /// @returns true if completed successfully, false on timeout or if device not initialized
    /// @note Waits for the most recent write() operation to finish
    /// @code{.cpp}
    /// spi.write(lane0, lane1, lane2, lane3);
    /// spi.wait();  // Block until transmission completes
    /// @endcode
    bool wait(uint32_t timeout_ms = 0xFFFFFFFF);

    /// @brief Get access to underlying device (for advanced operations)
    /// @returns Pointer to device (nullptr if !ok())
    spi::MultiLaneDevice* get() { return device.get(); }
    const spi::MultiLaneDevice* get() const { return device.get(); }

private:
    fl::unique_ptr<spi::MultiLaneDevice> device;
    bool is_ok;
    SPIError error_code;
};

// ============================================================================
// Type Aliases
// ============================================================================

using SpiTransaction = spi::Transaction;

template<typename T = void>
using SpiResult = spi::Result<T>;

} // namespace fl
