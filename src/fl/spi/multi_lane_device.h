#pragma once

/// @file spi/multi_lane_device.h
/// @brief Multi-lane SPI device for 2-8 independent LED strips

#include "fl/stl/stdint.h"
#include "fl/stl/vector.h"
#include "fl/stl/unique_ptr.h"
#include "fl/result.h"
#include "fl/stl/optional.h"
#include "fl/promise.h"  // for fl::Error
#include "fl/spi/config.h"
#include "fl/spi/lane.h"
#include "fl/numeric_limits.h"
#include "fl/spi/transaction.h"
#include "fl/spi/write_result.h"
#include "platforms/shared/spi_types.h"  // ok platform headers

namespace fl {
namespace spi {

// ============================================================================
// MultiLaneDevice - 1-8 independent data streams (HW transposed)
// ============================================================================

/// @brief Multi-lane SPI device (1-8 independent LED strips)
/// @details Manages one or more independent data streams that are transposed
///          and transmitted in parallel using hardware DMA (SpiHw1/2/4/8)
///
/// **Architecture:**
/// - Each lane has independent buffer (via Lane class)
/// - User writes to each lane independently
/// - flush() transposes all lanes and transmits via hardware
/// - Auto-selects SpiHw1 (1 lane), SpiHw2 (2 lanes), SpiHw4 (3-4 lanes), or SpiHw8 (5-8 lanes)
///
/// **Example:**
/// @code
/// MultiLaneDevice::Config config;
/// config.clock_pin = 18;
/// config.data_pins = {23, 22, 21, 19};  // 4 lanes
/// MultiLaneDevice spi(config);
/// spi.begin();
///
/// spi.lane(0).write(data0, size0);
/// spi.lane(1).write(data1, size1);
/// auto result = spi.flush();
/// if (result.ok()) {
///     spi.wait();  // Block until transmission completes
/// }
/// @endcode
class MultiLaneDevice {
public:
    /// @brief Configuration for multi-lane SPI
    struct Config {
        uint8_t clock_pin;              ///< Shared clock pin (SCK)
        fl::vector<uint8_t> data_pins;  ///< Data pins (1-8 pins)
        uint32_t clock_speed_hz;        ///< Clock speed in Hz (0xffffffff = as fast as possible)
        uint8_t mode;                   ///< SPI mode (CPOL/CPHA)

        Config() : clock_pin(0xFF), clock_speed_hz(0xffffffff), mode(0) {}
    };

    /// @brief Construct multi-lane device
    /// @param config Configuration with 1-8 data pins
    explicit MultiLaneDevice(const Config& config);

    /// @brief Destructor - releases hardware resources
    ~MultiLaneDevice();

    // Disable copy/move
    MultiLaneDevice(const MultiLaneDevice&) = delete;
    MultiLaneDevice& operator=(const MultiLaneDevice&) = delete;
    MultiLaneDevice(MultiLaneDevice&&) = delete;
    MultiLaneDevice& operator=(MultiLaneDevice&&) = delete;

    // ========== Initialization ==========

    /// @brief Initialize hardware
    /// @returns Optional error (nullopt on success)
    /// @note Auto-selects SpiHw1/2/4/8 based on number of data pins
    fl::optional<fl::Error> begin();

    /// @brief Shutdown hardware and release resources
    /// @note Waits for pending transmissions to complete
    void end();

    /// @brief Check if device is initialized
    /// @returns true if initialized and ready
    bool isReady() const;

    // ========== Lane Access ==========

    /// @brief Get access to a specific lane
    /// @param lane_id Lane index (0 to numLanes()-1)
    /// @returns Reference to Lane object
    /// @note Panics if lane_id is out of range
    Lane& lane(size_t lane_id);

    /// @brief Get number of lanes
    /// @returns Number of data pins (2-8)
    size_t numLanes() const;

    // ========== Transmission ==========

    /// @brief Flush all lanes (transpose and transmit)
    /// @returns Result indicating success or error (Result<void>)
    /// @note Transposes all lane buffers and transmits via hardware DMA
    /// @note Clears all lane buffers after transmission starts
    /// @note All non-empty lanes MUST have identical sizes - operation fails with error if sizes differ
    /// @note Zero-padding is NOT performed - size validation prevents unreliable chipset-specific padding issues
    /// @note Transaction API not yet implemented - call waitComplete() manually after flush()
    Result<void> flush();

    /// @brief Wait for pending transmission to complete
    /// @param timeout_ms Maximum time to wait (default: forever)
    /// @returns true if completed, false on timeout
    bool waitComplete(uint32_t timeout_ms = (fl::numeric_limits<uint32_t>::max)());

    /// @brief Convenience method - wait for transmission to complete
    /// @returns true if completed, false on timeout
    /// @note Alias for waitComplete() with infinite timeout
    bool wait() { return waitComplete(); }

    /// @brief Check if transmission is in progress
    /// @returns true if busy, false if idle
    bool isBusy() const;

    // ========== High-Level Write API ==========
    // This method provides a simple, type-safe way to write to multiple lanes
    // simultaneously. Each lane can have a different buffer size.
    //
    // Usage: auto result = spi->write(lane0, lane1, lane2, lane3);
    //
    // Accepts fl::span<const uint8_t> or any type convertible to it (arrays, vectors, etc.)
    //
    // This method automatically:
    // 1. Waits for any previous transmission to complete (waitComplete)
    // 2. Writes all lane data atomically (single call to writeImpl)
    // 3. Flushes to start the transmission **asynchronously**
    //
    // Transmission happens in the background. Call wait() to block until complete.

    /// @brief Convenience method - write multiple lanes in parallel (variadic template)
    /// @tparam Spans Variadic template parameter pack (all must be convertible to fl::span<const uint8_t>)
    /// @param lanes Lane data as spans (can be 1-8 lanes)
    /// @returns WriteResult with ok=true on success, or ok=false with error message
    /// @note Automatically waits for previous transmission, then starts new one **asynchronously**
    /// @note Call wait() to block until transmission completes
    /// @note **IMPORTANT**: All lanes MUST have identical sizes. Operation fails if sizes differ.
    /// @note Users must handle chipset-specific padding at application level before calling write()
    /// @code{.cpp}
    /// fl::array<uint8_t, 16> lane0 = {...};
    /// fl::array<uint8_t, 16> lane1 = {...};  // Same size as lane0!
    /// fl::array<uint8_t, 16> lane2 = {...};  // Same size as lane0!
    /// fl::array<uint8_t, 16> lane3 = {...};  // Same size as lane0!
    ///
    /// // Async usage - transmission happens in background
    /// auto result = spi->write(lane0, lane1, lane2, lane3);
    /// if (!result.ok) {
    ///     FL_WARN("Write failed: " << result.error);
    /// }
    /// // ... do other work ...
    ///
    /// // Sync usage - wait for completion
    /// spi->write(lane0, lane1, lane2, lane3);
    /// spi->wait();  // Block until done
    /// @endcode
    template<typename... Spans>
    WriteResult write(Spans&&... lanes) {
        // Unpack variadic template into stack-allocated FixedVector
        fl::span<const uint8_t> lane_spans[] = {fl::forward<Spans>(lanes)...};
        fl::FixedVector<fl::span<const uint8_t>, MAX_SPI_LANES> lane_vec;

        // Copy spans into vector
        for (size_t i = 0; i < sizeof...(lanes); i++) {
            lane_vec.push_back(lane_spans[i]);
        }

        // Single atomic write operation
        return writeImpl(fl::span<const fl::span<const uint8_t>>(lane_vec.data(), lane_vec.size()));
    }

    // ========== Configuration ==========

    /// @brief Get current configuration
    /// @returns Reference to config
    const Config& getConfig() const;

private:
    /// @brief Internal implementation - write all lanes atomically
    /// @param lane_data Span of spans containing lane data
    /// @note Waits for previous transmission, writes all lanes, then flushes
    /// @returns WriteResult with ok=true on success, or ok=false with error message
    WriteResult writeImpl(fl::span<const fl::span<const uint8_t>> lane_data);

    struct Impl;
    fl::unique_ptr<Impl> pImpl;
};

} // namespace spi
} // namespace fl
