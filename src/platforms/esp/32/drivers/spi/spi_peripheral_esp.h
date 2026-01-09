/// @file spi_peripheral_esp.h
/// @brief Real ESP32 SPI peripheral interface (thin header)
///
/// This header provides a thin interface to the ESP32 SPI Master hardware.
/// All implementation details and ESP-IDF dependencies are in the .cpp file.
///
/// ## Design Philosophy
///
/// This implementation follows the "thin wrapper" pattern:
/// - NO business logic (pure delegation to ESP-IDF)
/// - NO state validation beyond what ESP-IDF provides
/// - NO performance overhead (inline-able calls)
/// - ALL logic stays in ChannelEngineSpi (testable via mock)
///
/// ## Thread Safety
///
/// Thread safety is inherited from ESP-IDF SPI Master driver:
/// - initializeBus() NOT thread-safe (call once during setup)
/// - queueTransaction() can be called from ISR context (ISR-safe)
/// - Other methods NOT thread-safe (caller synchronizes)
///
/// ## Error Handling
///
/// All methods return bool for success/failure:
/// - true: Operation succeeded (ESP_OK)
/// - false: Operation failed (any ESP-IDF error code)
///
/// Detailed error codes are NOT propagated through the interface.
/// The ChannelEngineSpi logs errors internally for debugging.

#pragma once

#ifdef ESP32

#include "platforms/esp/32/feature_flags/enabled.h"

#if FASTLED_ESP32_HAS_CLOCKLESS_SPI

#include "ispi_peripheral.h"
#include "fl/singleton.h"

namespace fl {
namespace detail {

// Forward declaration for Singleton friendship
class SpiPeripheralESPImpl;

//=============================================================================
// Real Hardware Peripheral Interface
//=============================================================================

/// @brief Real ESP32 SPI peripheral interface
///
/// Thin wrapper around ESP-IDF SPI Master APIs. All methods delegate
/// directly to ESP-IDF with minimal overhead.
///
/// This is an abstract interface - use instance() to access the singleton.
class SpiPeripheralESP : public ISpiPeripheral {
public:
    /// @brief Get the singleton instance
    /// @return Reference to the singleton peripheral
    ///
    /// Mirrors the hardware constraint that there is a limited number of SPI hosts.
    static SpiPeripheralESP& instance();

    /// @brief Destructor - frees ESP-IDF device handle
    ~SpiPeripheralESP() override;

    // Prevent copying (handle cannot be shared)
    SpiPeripheralESP(const SpiPeripheralESP&) = delete;
    SpiPeripheralESP& operator=(const SpiPeripheralESP&) = delete;

    //=========================================================================
    // ISpiPeripheral Interface Implementation
    //=========================================================================

    bool initializeBus(const SpiBusConfig& config) override = 0;
    bool addDevice(const SpiDeviceConfig& config) override = 0;
    bool removeDevice() override = 0;
    bool freeBus() override = 0;
    bool isInitialized() const override = 0;
    bool queueTransaction(const SpiTransaction& trans) override = 0;
    bool pollTransaction(uint32_t timeout_ms) override = 0;
    bool registerCallback(void* callback, void* user_ctx) override = 0;
    uint8_t* allocateDma(size_t size) override = 0;
    void freeDma(uint8_t* buffer) override = 0;
    void delay(uint32_t ms) override = 0;
    uint64_t getMicroseconds() override = 0;

protected:
    /// @brief Protected constructor for singleton
    SpiPeripheralESP() = default;
};

} // namespace detail
} // namespace fl

#endif // FASTLED_ESP32_HAS_CLOCKLESS_SPI
#endif // ESP32
