#pragma once

/// @file spi_hw_1.h
/// @brief Platform-agnostic 1-lane hardware SPI interface
///
/// This file defines the abstract interface for 1-lane (single-lane) SPI hardware.
/// Platform-specific implementations (ESP32, RP2040, etc.) inherit from this
/// interface to provide hardware SPI support.
///
/// **Implementation Notes:**
/// - ESP32: Uses true async DMA via spi_device_queue_trans()
/// - Other platforms: May use synchronous polling or DMA depending on capabilities

#include "ftl/vector.h"
#include "ftl/span.h"
#include "ftl/stdint.h"
#include "ftl/limits.h"
#include "platforms/shared/spi_types.h"

namespace fl {

class SpiHw1;

/// Abstract interface for platform-specific 1-lane hardware SPI
///
/// Platform implementations (ESP32, RP2040, etc.) inherit from this interface
/// and provide concrete implementations of all virtual methods.
///
/// Naming: "SpiHw1" = SPI Hardware 1-lane
class SpiHw1 {
public:
    virtual ~SpiHw1() = default;

    /// Platform-agnostic configuration structure
    struct Config {
        uint8_t bus_num;           ///< SPI bus number (platform-specific numbering)
        uint32_t clock_speed_hz;   ///< Clock frequency in Hz
        int8_t clock_pin;          ///< SCK GPIO pin
        int8_t data_pin;           ///< MOSI GPIO pin
        uint32_t max_transfer_sz;  ///< Max bytes per transfer

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

    /// Acquire writable DMA buffer for zero-copy transmission
    /// @param size Number of bytes needed
    /// @returns DMABuffer containing buffer span or error code
    /// @note Automatically waits (calls waitComplete()) if previous transmission active
    /// @note Buffer remains valid until waitComplete() is called
    virtual DMABuffer acquireDMABuffer(size_t size) = 0;

    /// Transmit data from previously acquired DMA buffer
    /// @param mode Transmission mode hint (SYNC or ASYNC)
    /// @returns true if transmitted successfully, false on error
    /// @note ESP32: Queues async DMA transaction (non-blocking)
    /// @note Must call acquireDMABuffer() before this
    virtual bool transmit(TransmitMode mode = TransmitMode::ASYNC) = 0;

    /// Wait for current transmission to complete (blocking)
    /// @param timeout_ms Maximum wait time in milliseconds
    /// @returns true if completed, false on timeout
    /// @note **Releases DMA buffer** - buffer acquired via acquireDMABuffer() becomes invalid
    virtual bool waitComplete(uint32_t timeout_ms = (fl::numeric_limits<uint32_t>::max)()) = 0;

    /// Check if a transmission is currently in progress
    /// @returns true if busy, false if idle
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

    /// Get all available 1-lane hardware SPI devices on this platform
    /// @returns Reference to static vector of available devices
    /// @note Cached - only allocates once on first call
    /// @note Returns empty vector if platform doesn't support hardware SPI
    /// @note Returns bare pointers - instances are alive forever (static lifetime)
    static const fl::vector<SpiHw1*>& getAll();

private:
    /// Platform-specific factory implementation (weak linkage)
    /// Each platform overrides this with strong definition
    /// @returns Vector of platform-specific instances
    static fl::vector<SpiHw1*> createInstances();
};

}  // namespace fl
