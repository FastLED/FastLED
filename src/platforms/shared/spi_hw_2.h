#pragma once

/// @file spi_hw_2.h
/// @brief Platform-agnostic 2-lane hardware SPI interface
///
/// This file defines the abstract interface that all platform-specific
/// 2-lane (dual-lane) SPI hardware must implement. It enables the generic
/// DualSPIDevice to work across different platforms (ESP32, RP2040, etc.)
/// without knowing platform-specific implementation details.

#include "fl/stl/vector.h"
#include "fl/stl/span.h"
#include "fl/stl/stdint.h"
#include "fl/stl/limits.h"
#include "fl/stl/shared_ptr.h"
#include "platforms/shared/spi_types.h"
#include "platforms/shared/spi_hw_base.h"

namespace fl {

class SpiHw2;

/// Abstract interface for platform-specific 2-lane hardware SPI
///
/// Platform implementations (ESP32, RP2040, etc.) inherit from this interface
/// and provide concrete implementations of all virtual methods.
///
/// Naming: "SpiHw2" = SPI Hardware 2-lane
class SpiHw2 : public SpiHwBase {
public:
    virtual ~SpiHw2() = default;

    /// Platform-agnostic configuration structure
    struct Config {
        uint8_t bus_num;           ///< SPI bus number (platform-specific numbering)
        uint32_t clock_speed_hz;   ///< Clock frequency in Hz
        int8_t clock_pin;          ///< SCK GPIO pin
        int8_t data0_pin;          ///< D0/MOSI GPIO pin
        int8_t data1_pin;          ///< D1/MISO GPIO pin
        uint32_t max_transfer_sz;  ///< Max bytes per transfer

        Config()
            : bus_num(0)
            , clock_speed_hz(20000000)
            , clock_pin(-1)
            , data0_pin(-1)
            , data1_pin(-1)
            , max_transfer_sz(65536) {}
    };

    /// Initialize SPI peripheral with given configuration
    /// @param config Hardware configuration
    /// @returns true on success, false on error
    /// @note Implementation should configure dual mode based on active pins
    virtual bool begin(const Config& config) = 0;

    /// @brief Polymorphic begin() override for SpiHwBase interface
    /// @param config Type-erased config pointer (must be Config*)
    /// @returns true on success, false on error
    bool begin(const void* config) override {
        return begin(*static_cast<const Config*>(config));
    }

    /// @brief Get lane count for polymorphic interface
    /// @returns Always returns 2 for dual-lane SPI
    uint8_t getLaneCount() const override { return 2; }

    /// Shutdown SPI peripheral and release resources
    /// @note Should wait for any pending transmissions to complete
    virtual void end() override = 0;

    /// Acquire writable DMA buffer for zero-copy transmission
    /// @param size Number of bytes needed
    /// @returns DMABuffer containing buffer span or error code
    /// @note Automatically waits (calls waitComplete()) if previous transmission active
    /// @note Buffer remains valid until waitComplete() is called
    virtual DMABuffer acquireDMABuffer(size_t size) override = 0;

    /// Transmit data from previously acquired DMA buffer
    /// @param mode Transmission mode hint (SYNC or ASYNC)
    /// @returns true if transmitted successfully, false on error
    /// @note Must call acquireDMABuffer() before this
    virtual bool transmit(TransmitMode mode = TransmitMode::ASYNC) override = 0;

    /// Wait for current transmission to complete (blocking)
    /// @param timeout_ms Maximum wait time in milliseconds
    /// @returns true if completed, false on timeout
    /// @note **Releases DMA buffer** - buffer acquired via acquireDMABuffer() becomes invalid
    virtual bool waitComplete(uint32_t timeout_ms = (fl::numeric_limits<uint32_t>::max)()) override = 0;

    /// Check if a transmission is currently in progress
    /// @returns true if busy, false if idle
    virtual bool isBusy() const override = 0;

    /// Get initialization status
    /// @returns true if initialized, false otherwise
    virtual bool isInitialized() const override = 0;

    /// Get the SPI bus number/ID for this controller
    /// @returns SPI bus number (e.g., 2 or 3 for ESP32), or -1 if not assigned
    virtual int getBusId() const override = 0;

    /// Get the platform-specific peripheral name for this controller
    /// @returns Human-readable peripheral name (e.g., "HSPI", "VSPI", "SPI0")
    /// @note Primarily for debugging, logging, and error messages
    /// @note Returns "Unknown" if not assigned
    virtual const char* getName() const override = 0;

    /// Get all available 2-lane hardware SPI devices on this platform
    /// @returns Reference to static vector of available devices
    /// @note Cached - only allocates once on first call
    /// @note Returns empty vector if platform doesn't support 2-lane SPI
    /// @note Instances are managed via shared_ptr for safe lifetime management
    static const fl::vector<fl::shared_ptr<SpiHw2>>& getAll();

    /// Register a platform-specific instance
    /// @param instance Shared pointer to platform-specific SpiHw2 instance
    /// @note Called by platform implementations during static initialization
    static void registerInstance(fl::shared_ptr<SpiHw2> instance);

    /// Remove a registered instance
    /// @param instance The shared pointer to remove from the registry
    /// @returns true if removed, false if not found
    static bool removeInstance(const fl::shared_ptr<SpiHw2>& instance);

    /// Clear all registered instances (primarily for testing)
    /// @note Use with caution - invalidates all references returned by getAll()
    static void clearInstances();
};

}  // namespace fl
