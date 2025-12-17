#pragma once

/// @file spi_hw_base.h
/// @brief Abstract base interface for all SPI hardware controllers
///
/// This file defines the polymorphic base interface that enables type-safe
/// storage and usage of SPI hardware controllers without type erasure.
/// All SpiHw1/2/4/8 classes inherit from this interface.
///
/// **Design Rationale:**
/// - Replaces type-erased `shared_ptr<void>` with proper OOP polymorphism
/// - Eliminates manual RTTI via backend_type discriminators
/// - Removes casting boilerplate (`static_pointer_cast` everywhere)
/// - Type-safe, compiler-checked, standard C++ design
///
/// **Usage:**
/// ```cpp
/// fl::shared_ptr<SpiHwBase> backend = SpiHw2::getAll()[0];
/// backend->transmit(TransmitMode::ASYNC);  // Clean polymorphic call
/// ```

#include "fl/stl/stdint.h"
#include "fl/stl/limits.h"
#include "platforms/shared/spi_types.h"

namespace fl {

/// @brief Abstract base interface for all SPI hardware controllers
/// @details Provides polymorphic interface for 1/2/4/8-lane SPI hardware.
///          All concrete SpiHwN classes inherit from this interface to enable
///          unified storage and usage without type erasure or casting.
class SpiHwBase {
public:
    virtual ~SpiHwBase() = default;

    /// Initialize SPI peripheral with configuration
    /// @param config Platform-specific config (cast to appropriate Config type)
    /// @returns true on success, false on error
    /// @note Concrete classes provide type-safe overloads: begin(const Config&)
    virtual bool begin(const void* config) = 0;

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
    /// @returns SPI bus number (platform-specific), or -1 if not assigned
    virtual int getBusId() const = 0;

    /// Get the platform-specific peripheral name for this controller
    /// @returns Human-readable peripheral name (e.g., "HSPI", "VSPI")
    /// @note Primarily for debugging, logging, and error messages
    /// @note Returns "Unknown" if not assigned
    virtual const char* getName() const = 0;

    /// Get the number of data lanes for this controller
    /// @returns Lane count: 1, 2, 4, or 8
    /// @note Used to determine hardware capabilities without downcasting
    virtual uint8_t getLaneCount() const = 0;
};

} // namespace fl
