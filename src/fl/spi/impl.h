#pragma once

/// @file spi/impl.h
/// @brief Private implementation details for fl::spi::Device
/// @note This header is for internal use only (pImpl pattern)

#include "fl/spi.h"
#include "platforms/shared/spi_bus_manager.h"  // ok platform headers
#include "platforms/shared/spi_types.h"  // ok platform headers
#include "platforms/shared/spi_hw_base.h"  // ok platform headers
#include "fl/log.h"
#include "fl/numeric_limits.h"

namespace fl {
namespace spi {

/// @brief Private implementation data for Device class
struct Device::Impl {
    Config config;                      ///< Device configuration
    SPIBusHandle bus_handle;            ///< Handle from SPIBusManager
    bool initialized;                   ///< Whether hardware is initialized

    /// @brief State for async operations
    struct AsyncState {
        bool active;                    ///< Whether an async operation is in progress
        const uint8_t* tx_buffer;       ///< TX buffer pointer (caller-owned)
        uint8_t* rx_buffer;             ///< RX buffer pointer (caller-owned)
        size_t size;                    ///< Transfer size in bytes
        uint32_t start_time;            ///< Start time for timeout tracking
    } async_state;

    /// @brief Platform-specific backend pointer (for single-lane SPI)
    /// @note For multi-lane SPI, the backend is managed by SPIBusManager
    /// @note For SINGLE_SPI mode (passthrough), this Device owns the controller
    fl::shared_ptr<SpiHwBase> hw_backend;

    /// @brief True if this Device owns the hw_backend (SINGLE_SPI mode)
    bool owns_backend;

    /// @brief Constructor
    explicit Impl(const Config& cfg)
        : config(cfg)
        , bus_handle()
        , initialized(false)
        , async_state{false, nullptr, nullptr, 0, 0}
        , hw_backend(nullptr)
        , owns_backend(false) {}

    /// @brief Destructor
    ~Impl() {
        FL_LOG_SPI("Device::Impl: Destructor called");
        // Members will be destroyed in reverse order of declaration:
        // 1. owns_backend (bool) - trivial
        // 2. hw_backend (void*) - trivial (pointer not owned here)
        // 3. async_state (struct) - trivial
        // 4. initialized (bool) - trivial
        // 5. bus_handle (struct) - trivial
        // 6. config (Config) - contains fl::vector which might cause issues
        FL_LOG_SPI("Device::Impl: Destructor complete");
    }
};

/// @brief Private implementation data for Transaction class
struct Transaction::Impl {
    Device* device;                     ///< Back-reference to device
    bool completed;                     ///< Whether transaction has completed
    bool cancelled;                     ///< Whether transaction was cancelled
    fl::optional<fl::Error> result;     ///< Result of the transaction (nullopt = success)
    uint32_t timeout_ms;                ///< Timeout value in milliseconds

    // Platform-specific completion tracking
#ifdef ESP32
    void* notify_task;                  ///< FreeRTOS task handle (TaskHandle_t)
#endif

    /// @brief Constructor
    explicit Impl(Device* dev)
        : device(dev)
        , completed(false)
        , cancelled(false)
        , result(fl::nullopt)  // nullopt = success
        , timeout_ms((fl::numeric_limits<uint32_t>::max)())
#ifdef ESP32
        , notify_task(nullptr)
#endif
    {}
};

} // namespace spi
} // namespace fl
