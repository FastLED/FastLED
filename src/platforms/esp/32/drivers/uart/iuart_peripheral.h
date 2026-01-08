/// @file iuart_peripheral.h
/// @brief Virtual interface for UART peripheral hardware abstraction
///
/// This interface enables mock injection for unit testing of the UART LED driver.
/// It abstracts all ESP-IDF UART API calls into a clean interface that can be:
/// - Implemented by UartPeripheralEsp (real hardware delegate)
/// - Implemented by UartPeripheralMock (unit test simulation)
///
/// ## Design Rationale
///
/// The UART LED driver contains wave8 encoding logic, buffer management, and
/// transmission coordination. This logic should be unit testable without
/// requiring real ESP32 hardware. By extracting a virtual peripheral interface,
/// we achieve:
///
/// 1. **Testability**: Mock implementation enables host-based unit tests
/// 2. **Separation of Concerns**: Hardware delegation vs. business logic
/// 3. **Performance**: Virtual dispatch adds only ~2-3 CPU cycles overhead
/// 4. **Maintainability**: Clear contract between engine and hardware
///
/// ## Interface Contract
///
/// - All methods return `bool` (true = success, false = error)
/// - Methods mirror ESP-IDF UART API semantics exactly
/// - No ESP-IDF types leak into interface
/// - Thread safety: Caller responsible for synchronization
///
/// ## UART Frame Structure (8N1)
///
/// Each transmitted byte becomes a 10-bit frame:
/// ```
/// [START] [B0] [B1] [B2] [B3] [B4] [B5] [B6] [B7] [STOP]
///   LOW    D0   D1   D2   D3   D4   D5   D6   D7   HIGH
/// ```
/// - Start bit: Always LOW (automatic)
/// - Stop bit: Always HIGH (automatic)
/// - Data bits: LSB first (B0 transmitted first)
///
/// This automatic framing simplifies LED waveform generation compared to
/// manual bit stuffing.

#pragma once

// This interface is platform-agnostic (no ESP32 guard)
// - UartPeripheralEsp requires ESP32 (real hardware)
// - UartPeripheralMock works on all platforms (testing)

#include "fl/stl/stdint.h"
#include "fl/stl/cstddef.h"

namespace fl {

//=============================================================================
// Configuration Structures
//=============================================================================

/// @brief UART peripheral configuration
///
/// Encapsulates all parameters needed to initialize the UART hardware.
/// Maps directly to ESP-IDF's uart_config_t structure.
struct UartConfig {
    uint32_t mBaudRate;          ///< Baud rate (e.g., 3200000 for 3.2 Mbps)
    int mTxPin;                  ///< GPIO pin for TX output
    int mRxPin;                  ///< GPIO pin for RX (typically -1 for TX-only)
    uint32_t mTxBufferSize;      ///< TX ring buffer size (0 = blocking mode)
    uint32_t mRxBufferSize;      ///< RX ring buffer size (typically 0 for LED control)
    uint8_t mStopBits;           ///< Stop bits: 1 or 2 (UART_STOP_BITS_1=1, UART_STOP_BITS_2=2)
    int mUartNum;                ///< UART peripheral number (0, 1, or 2)

    /// @brief Default constructor (for mock testing)
    UartConfig()
        : mBaudRate(0),
          mTxPin(-1),
          mRxPin(-1),
          mTxBufferSize(0),
          mRxBufferSize(0),
          mStopBits(1),
          mUartNum(0) {}

    /// @brief Constructor with all parameters
    UartConfig(uint32_t baud_rate,
               int tx_pin,
               int rx_pin,
               uint32_t tx_buffer_size,
               uint32_t rx_buffer_size,
               uint8_t stop_bits,
               int uart_num)
        : mBaudRate(baud_rate),
          mTxPin(tx_pin),
          mRxPin(rx_pin),
          mTxBufferSize(tx_buffer_size),
          mRxBufferSize(rx_buffer_size),
          mStopBits(stop_bits),
          mUartNum(uart_num) {}
};

//=============================================================================
// Virtual Peripheral Interface
//=============================================================================

/// @brief Virtual interface for UART peripheral hardware abstraction
///
/// Pure virtual interface that abstracts all ESP-IDF UART operations.
/// Implementations:
/// - UartPeripheralEsp: Thin wrapper around ESP-IDF APIs (real hardware)
/// - UartPeripheralMock: Simulation for host-based unit tests
///
/// ## Usage Pattern
/// ```cpp
/// // Create peripheral (real or mock)
/// IUartPeripheral* peripheral = new UartPeripheralEsp();
///
/// // Configure
/// UartConfig config = {
///     .mBaudRate = 3200000,    // 3.2 Mbps
///     .mTxPin = 17,
///     .mRxPin = -1,
///     .mTxBufferSize = 4096,
///     .mRxBufferSize = 0,
///     .mStopBits = 1,
///     .mUartNum = 1
/// };
/// if (!peripheral->initialize(config)) { /* error */ }
///
/// // Transmit data
/// uint8_t data[] = {0xFF, 0x00, 0xAA};
/// peripheral->writeBytes(data, sizeof(data));
///
/// // Wait for completion
/// peripheral->waitTxDone(1000);
///
/// // Cleanup
/// peripheral->deinitialize();
/// delete peripheral;
/// ```
class IUartPeripheral {
public:
    virtual ~IUartPeripheral() = default;

    //=========================================================================
    // Lifecycle Methods
    //=========================================================================

    /// @brief Initialize UART peripheral with configuration
    /// @param config Hardware configuration (baud rate, pins, buffer sizes)
    /// @return true on success, false on error
    ///
    /// Maps to ESP-IDF: uart_driver_install() + uart_set_pin() + uart_param_config()
    ///
    /// This method:
    /// - Creates the UART driver instance
    /// - Configures TX/RX GPIO pins
    /// - Sets baud rate and framing (8N1 or 8N2)
    /// - Allocates TX/RX ring buffers
    ///
    /// Call once during initialization. Must succeed before any other
    /// methods can be used.
    virtual bool initialize(const UartConfig& config) = 0;

    /// @brief Deinitialize UART peripheral and release resources
    ///
    /// Maps to ESP-IDF: uart_driver_delete()
    ///
    /// Call after all transmissions are complete. Frees TX/RX buffers
    /// and releases UART hardware.
    virtual void deinitialize() = 0;

    /// @brief Check if peripheral is initialized
    /// @return true if initialized, false otherwise
    ///
    /// Used to detect if peripheral was reset (for testing).
    /// Production hardware: Always returns true after initialize() succeeds.
    /// Mock implementation: Returns false after deinitialize() is called.
    virtual bool isInitialized() const = 0;

    //=========================================================================
    // Transmission Methods
    //=========================================================================

    /// @brief Write bytes to UART TX buffer
    /// @param data Pointer to data buffer
    /// @param length Number of bytes to transmit
    /// @return true on success, false on error
    ///
    /// Maps to ESP-IDF: uart_write_bytes()
    ///
    /// This method copies data to the UART TX ring buffer. Transmission
    /// happens asynchronously via hardware. Use waitTxDone() to block until
    /// transmission completes.
    ///
    /// Behavior:
    /// - If TX buffer has space: Returns immediately after copying
    /// - If TX buffer is full: Blocks until space available (or timeout)
    ///
    /// Each byte is automatically framed with start bit (LOW) and stop bit (HIGH).
    virtual bool writeBytes(const uint8_t* data, size_t length) = 0;

    /// @brief Wait for all queued bytes to be transmitted
    /// @param timeout_ms Timeout in milliseconds (0 = non-blocking poll)
    /// @return true if transmission complete, false on timeout or error
    ///
    /// Maps to ESP-IDF: uart_wait_tx_done()
    ///
    /// Blocks until all bytes in the TX ring buffer and TX FIFO have been
    /// transmitted, or timeout occurs.
    ///
    /// Returns true if:
    /// - All transmissions complete within timeout
    /// - No transmissions are active (immediate return)
    ///
    /// Returns false if:
    /// - Timeout occurs before completion
    /// - Hardware error occurs during transmission
    virtual bool waitTxDone(uint32_t timeout_ms) = 0;

    //=========================================================================
    // State Queries
    //=========================================================================

    /// @brief Check if UART is busy transmitting
    /// @return true if transmission in progress, false if idle
    ///
    /// Used to poll transmission status without blocking. Equivalent to
    /// calling waitTxDone(0) but more explicit.
    virtual bool isBusy() const = 0;

    /// @brief Get current UART configuration
    /// @return Reference to current configuration
    ///
    /// Returns the configuration passed to initialize(). Useful for
    /// debugging and validation.
    virtual const UartConfig& getConfig() const = 0;
};

} // namespace fl
