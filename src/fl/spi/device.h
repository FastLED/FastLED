#pragma once

/// @file spi/device.h
/// @brief SPI Device class for single-channel communication
///
/// This file contains the main Device class for SPI communication.

#include "fl/stl/stdint.h"
#include "fl/stl/span.h"
#include "fl/stl/unique_ptr.h"
#include "fl/result.h"
#include "fl/stl/optional.h"
#include "fl/promise.h"  // for fl::Error
#include "fl/numeric_limits.h"
#include "fl/spi/config.h"
#include "fl/spi/transaction.h"
#include "platforms/shared/spi_types.h"  // ok platform headers - DMABuffer, SPIError, TransmitMode

namespace fl {
namespace spi {

// ============================================================================
// Main Device Class
// ============================================================================

/// @brief Single-channel SPI device interface (TX-only, transaction-based, optimized for FastLED)
/// @details Provides easy-to-use API for SPI communication with two levels:
/// 1. Transaction API (writeAsync) - Primary interface, returns Transaction handles
/// 2. Zero-Copy DMA (acquireBuffer/transmit) - Expert API for maximum performance
///
/// **TX-Only, Transaction-Based Design:**
/// This SPI implementation is optimized for LED strip output (WS2812, APA102, SK6812, etc.)
/// and only supports transmission (MOSI). Read operations (MISO) are not supported because:
/// - LED strips are receive-only devices (no status/readback)
/// - Removing RX support simplifies hardware configuration
/// - Allows more efficient DMA and buffer management
///
/// **Transaction API (Simpler Implementation):**
/// All operations use the transaction-based API (`writeAsync()`) which returns a Transaction
/// handle. This provides a single, consistent interface that's easier to implement and use:
/// - Async by default, but can be made blocking by calling `wait()` immediately
/// - Consistent error handling through Result<Transaction>
/// - Natural fit for DMA-based hardware
///
/// @note This class uses RAII - hardware is released on destruction
/// @note Non-copyable, non-movable (owns hardware resources)
class Device {
public:
    /// @brief Construct SPI device with configuration
    /// @param config Pin and speed configuration
    explicit Device(const Config& config);

    /// @brief Destructor - releases hardware resources
    /// @note Waits for pending operations to complete
    ~Device();

    // ========== Initialization ==========

    /// @brief Initialize the SPI hardware
    /// @returns Result indicating success or error
    /// @note Must be called before any communication methods
    fl::optional<fl::Error> begin();

    /// @brief Shutdown the SPI hardware and release resources
    /// @note Waits for pending operations to complete
    void end();

    /// @brief Check if device is initialized and ready for use
    /// @returns true if initialized, false otherwise
    bool isReady() const;

    // ========== Transaction API (Primary Interface) ==========

    /// @brief Begin asynchronous write operation (returns immediately)
    /// @param data Data to transmit (caller must keep alive until transaction completes)
    /// @param size Number of bytes to transmit
    /// @returns Result containing Transaction handle or error
    /// @note This is the primary transmission interface
    /// @warning Caller must ensure data remains valid until Transaction::wait() returns
    ///
    /// Example:
    /// @code
    /// uint8_t data[] = {0x01, 0x02, 0x03};
    /// auto result = spi.writeAsync(data, sizeof(data));
    /// if (result.ok()) {
    ///     result.value().wait();  // Wait for completion
    /// }
    /// @endcode
    Result<Transaction> writeAsync(const uint8_t* data, size_t size);

    // ========== Zero-Copy DMA API (Expert) ==========

    /// @brief Acquire DMA-capable buffer for zero-copy transmission
    /// @param size Number of bytes needed
    /// @returns DMABuffer (managed buffer) or error
    /// @note Buffer is backed by DMA-capable memory (PSRAM on ESP32)
    /// @note Buffer lifetime managed by shared_ptr
    DMABuffer acquireBuffer(size_t size);

    /// @brief Transmit from previously acquired DMA buffer
    /// @param buffer Buffer acquired via acquireBuffer()
    /// @param async If true, returns immediately; if false, waits for completion
    /// @returns Result indicating success or error
    /// @note Zero-copy: buffer is transmitted directly via DMA
    fl::optional<fl::Error> transmit(DMABuffer& buffer, bool async = true);

    /// @brief Wait for pending async operation to complete
    /// @param timeout_ms Maximum time to wait (default: forever)
    /// @returns true if completed, false on timeout
    bool waitComplete(uint32_t timeout_ms = (fl::numeric_limits<uint32_t>::max)());

    /// @brief Check if async operation is in progress
    /// @returns true if busy, false if idle
    bool isBusy() const;

    // ========== Configuration ==========

    /// @brief Update clock speed
    /// @param speed_hz New clock speed in Hz
    /// @returns Result indicating success or error
    /// @note Runtime updates not yet supported - new speed takes effect on next begin()
    /// @note To apply immediately: call end() then begin()
    fl::optional<fl::Error> setClockSpeed(uint32_t speed_hz);

    /// @brief Get current configuration
    /// @returns Reference to configuration structure
    const Config& getConfig() const;

private:
    friend class Transaction;  // Allow Transaction to access Device internals

    struct Impl;  // Forward declaration (pImpl pattern)
    fl::unique_ptr<Impl> pImpl;

    // Non-copyable, non-movable (owns hardware resources)
    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;
};

} // namespace spi
} // namespace fl
