/// @file uart_peripheral_esp.h
/// @brief Real ESP32 UART peripheral interface (thin header)
///
/// This header provides a thin interface to the ESP32 UART hardware.
/// All implementation details and ESP-IDF dependencies are in the .cpp file.
///
/// ## Design Philosophy
///
/// This implementation follows the "thin wrapper" pattern (mirroring PARLIO):
/// - NO business logic (pure delegation to ESP-IDF)
/// - NO state validation beyond what ESP-IDF provides
/// - NO performance overhead (inline-able calls)
/// - ALL logic stays in UartChannelEngine (testable via mock)
///
/// ## Thread Safety
///
/// Thread safety is inherited from ESP-IDF UART driver:
/// - initialize() NOT thread-safe (call once during setup)
/// - writeBytes() can be called from ISR context (ISR-safe)
/// - Other methods NOT thread-safe (caller synchronizes)
///
/// ## Error Handling
///
/// All methods return bool for success/failure:
/// - true: Operation succeeded (ESP_OK)
/// - false: Operation failed (any ESP-IDF error code)
///
/// Detailed error codes are NOT propagated through the interface.
/// The UartChannelEngine logs errors internally for debugging.

#pragma once

#ifdef ESP32

#include "iuart_peripheral.h"

namespace fl {

//=============================================================================
// Real Hardware Peripheral Interface
//=============================================================================

/// @brief Real ESP32 UART peripheral interface
///
/// Thin wrapper around ESP-IDF UART driver APIs. All methods delegate
/// directly to ESP-IDF with minimal overhead.
///
/// Unlike PARLIO, each UART instance is independent (no singleton pattern).
/// ESP32-C3 has 2 UARTs, ESP32-S3 has 3 UARTs - each can be instantiated
/// separately for multi-strip LED control.
class UartPeripheralEsp : public IUartPeripheral {
public:
    /// @brief Constructor
    UartPeripheralEsp();

    /// @brief Destructor - cleans up UART driver
    ~UartPeripheralEsp() override;

    // Prevent copying (UART handle cannot be shared)
    UartPeripheralEsp(const UartPeripheralEsp&) = delete;
    UartPeripheralEsp& operator=(const UartPeripheralEsp&) = delete;

    //=========================================================================
    // IUartPeripheral Interface Implementation
    //=========================================================================

    /// @brief Initialize UART peripheral with configuration
    /// @param config Hardware configuration (baud rate, pins, buffer sizes)
    /// @return true on success, false on error
    ///
    /// Maps to ESP-IDF: uart_param_config() + uart_driver_install() + uart_set_pin()
    bool initialize(const UartConfig& config) override;

    /// @brief Deinitialize UART peripheral and release resources
    ///
    /// Maps to ESP-IDF: uart_driver_delete()
    void deinitialize() override;

    /// @brief Check if peripheral is initialized
    /// @return true if initialized, false otherwise
    bool isInitialized() const override;

    /// @brief Write bytes to UART TX buffer
    /// @param data Pointer to data buffer
    /// @param length Number of bytes to transmit
    /// @return true on success, false on error
    ///
    /// Maps to ESP-IDF: uart_write_bytes()
    bool writeBytes(const uint8_t* data, size_t length) override;

    /// @brief Wait for all queued bytes to be transmitted
    /// @param timeout_ms Timeout in milliseconds (0 = non-blocking poll)
    /// @return true if transmission complete, false on timeout or error
    ///
    /// Maps to ESP-IDF: uart_wait_tx_done()
    bool waitTxDone(uint32_t timeout_ms) override;

    /// @brief Check if UART is busy transmitting
    /// @return true if transmission in progress, false if idle
    bool isBusy() const override;

    /// @brief Get current UART configuration
    /// @return Reference to current configuration
    const UartConfig& getConfig() const override;

private:
    UartConfig mConfig;        ///< Stored configuration
    bool mInitialized;         ///< Initialization state
    uint64_t mResetExpireTime; ///< Timestamp when reset period ends (microseconds)
};

} // namespace fl

#endif // ESP32
