#pragma once

/// @file spi/device_impl_base.h
/// @brief Shared implementation helpers for SPI device classes
/// @note This reduces code duplication between MultiLaneDevice and ParallelDevice

#include "fl/stl/stdint.h"
#include "fl/stl/shared_ptr.h"
#include "platforms/shared/spi_hw_base.h"  // ok platform headers

namespace fl {
namespace spi {

/// @brief Common functionality for device implementation (pImpl pattern)
/// @details Provides shared state and helper methods used by both
///          MultiLaneDevice and ParallelDevice implementations
struct DeviceImplBase {
    fl::shared_ptr<SpiHwBase> backend;  ///< Polymorphic SPI hardware backend (SpiHw1/2/4/8)
    bool initialized;                   ///< Whether hardware is initialized
    bool owns_backend;                  ///< Whether this device owns the backend (for cleanup)

    /// @brief Constructor
    DeviceImplBase()
        : backend()
        , initialized(false)
        , owns_backend(false) {}

    /// @brief Check if device is ready
    /// @returns true if initialized and backend is valid
    bool isReady() const {
        return initialized && backend != nullptr;
    }

    /// @brief Validate backend pointer
    /// @returns true if backend is valid
    bool hasBackend() const {
        return backend != nullptr;
    }

    /// @brief Clear backend state
    void clearBackend() {
        backend = nullptr;
        initialized = false;
    }
};

} // namespace spi
} // namespace fl
