#pragma once

/// @file spi_single.h
/// @brief Platform-agnostic Single-SPI interface (backwards compatibility layer)
///
/// This file defines the abstract interface for single-lane SPI hardware.
/// It provides a backwards-compatible proxy layer for existing SPI implementations.
///
/// **IMPORTANT COMPATIBILITY NOTE:**
/// This implementation currently uses BLOCKING transmitAsync() for backwards
/// compatibility with existing code. While the interface appears async, the
/// transmission completes synchronously before returning.
///
/// TODO: Convert to true async DMA implementation in the future.
/// This requires careful testing to ensure no regressions in existing code
/// that relies on the blocking behavior.

#include "fl/namespace.h"
#include "fl/vector.h"
#include "fl/span.h"
#include <stdint.h>
#include <stddef.h>

namespace fl {

class SPISingle;

/// Abstract interface for platform-specific Single-SPI hardware
///
/// Platform implementations (ESP32, RP2040, etc.) inherit from this interface
/// and provide concrete implementations of all virtual methods.
///
/// This serves as a backwards-compatible proxy layer for existing SPI code.
class SPISingle {
public:
    virtual ~SPISingle() = default;

    /// Platform-agnostic configuration structure
    struct Config {
        uint8_t bus_num;           ///< SPI bus number (platform-specific numbering)
        uint32_t clock_speed_hz;   ///< Clock frequency in Hz
        int8_t clock_pin;          ///< SCK GPIO pin
        int8_t data_pin;           ///< MOSI GPIO pin
        size_t max_transfer_sz;    ///< Max bytes per transfer

        Config()
            : bus_num(0)
            , clock_speed_hz(20000000)
            , clock_pin(-1)
            , data_pin(-1)
            , max_transfer_sz(65536) {}
    };

    /// Initialize SPI peripheral with given configuration
    /// @param config Hardware configuration
    /// @returns true on success, false on error
    virtual bool begin(const Config& config) = 0;

    /// Shutdown SPI peripheral and release resources
    /// @note Should wait for any pending transmissions to complete
    virtual void end() = 0;

    /// Queue transmission (currently BLOCKING for backwards compatibility)
    /// @param buffer Data buffer to transmit
    /// @returns true if transmitted successfully, false on error
    /// @note **COMPATIBILITY WARNING**: Despite the "Async" name, this is currently
    ///       BLOCKING and will not return until transmission completes.
    /// @note TODO: Convert to true async DMA implementation in the future.
    /// @note Buffer must remain valid until waitComplete() returns (currently immediate)
    virtual bool transmitAsync(fl::span<const uint8_t> buffer) = 0;

    /// Wait for current transmission to complete (blocking)
    /// @param timeout_ms Maximum wait time in milliseconds
    /// @returns true if completed, false on timeout
    /// @note Currently returns immediately as transmitAsync() is blocking
    virtual bool waitComplete(uint32_t timeout_ms = UINT32_MAX) = 0;

    /// Check if a transmission is currently in progress
    /// @returns true if busy, false if idle
    /// @note Currently always returns false as transmitAsync() is blocking
    virtual bool isBusy() const = 0;

    /// Get initialization status
    /// @returns true if initialized, false otherwise
    virtual bool isInitialized() const = 0;

    /// Get the SPI bus number/ID for this controller
    /// @returns SPI bus number (e.g., 2 or 3 for ESP32), or -1 if not assigned
    virtual int getBusId() const = 0;

    /// Get the platform-specific peripheral name for this controller
    /// @returns Human-readable peripheral name (e.g., "HSPI", "VSPI", "SPI0")
    /// @note Primarily for debugging, logging, and error messages
    /// @note Returns "Unknown" if not assigned
    virtual const char* getName() const = 0;

    /// Get all available Single-SPI devices on this platform
    /// @returns Reference to static vector of available devices
    /// @note Cached - only allocates once on first call
    /// @note Thread-safe via C++11 static local initialization
    /// @note Returns empty vector if platform doesn't support Single-SPI
    /// @note Returns bare pointers - instances are alive forever (static lifetime)
    static const fl::vector<SPISingle*>& getAll() {
        static fl::vector<SPISingle*> instances = createInstances();
        return instances;
    }

private:
    /// Platform-specific factory implementation (weak linkage)
    /// Each platform overrides this with strong definition
    /// @returns Vector of platform-specific instances
    static fl::vector<SPISingle*> createInstances();
};

}  // namespace fl
