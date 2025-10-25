#pragma once

/// @file spi/device_impl_base.h
/// @brief Shared implementation helpers for SPI device classes
/// @note This reduces code duplication between MultiLaneDevice and ParallelDevice

#include "fl/stdint.h"

namespace fl {
namespace spi {

/// @brief Common functionality for device implementation (pImpl pattern)
/// @details Provides shared state and helper methods used by both
///          MultiLaneDevice and ParallelDevice implementations
struct DeviceImplBase {
    void* backend;          ///< Type-erased backend pointer (SpiHw*/SpiIsr*/SpiBlock*)
    bool initialized;       ///< Whether hardware is initialized
    bool owns_backend;      ///< Whether this device owns the backend (for cleanup)

    /// @brief Constructor
    DeviceImplBase()
        : backend(nullptr)
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

/// @brief Helper for type-safe backend dispatch
/// @details Reduces boilerplate for calling methods on type-erased backends
/// @tparam T2 Backend type for width 2 (e.g., SpiHw2)
/// @tparam T4 Backend type for width 4 (e.g., SpiHw4)
/// @tparam T8 Backend type for width 8 (e.g., SpiHw8)
/// @tparam ReturnType Return type of the backend method
/// @tparam Func Callable type (lambda or function pointer)
template<typename T2, typename T4, typename T8, typename ReturnType, typename Func>
inline ReturnType dispatchBackend2_4_8(void* backend, uint8_t backend_type, Func&& func, ReturnType default_value) {
    if (backend_type == 2) {
        return func(static_cast<T2*>(backend));
    } else if (backend_type == 4) {
        return func(static_cast<T4*>(backend));
    } else if (backend_type == 8) {
        return func(static_cast<T8*>(backend));
    }
    return default_value;
}

} // namespace spi
} // namespace fl
