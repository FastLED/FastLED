/// @file uart_peripheral_esp.cpp
/// @brief Real ESP32 UART peripheral implementation
///
/// Thin wrapper around ESP-IDF UART driver APIs. This implementation
/// contains ZERO business logic - all methods delegate directly to ESP-IDF.

#ifdef ESP32

#include "uart_peripheral_esp.h"
#include "fl/log.h"
#include "fl/warn.h"
#include <Arduino.h>  // ok include - For micros()

// Include ESP-IDF headers ONLY in .cpp file
FL_EXTERN_C_BEGIN
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
FL_EXTERN_C_END

// ESP-IDF version compatibility for UART clock source
// - IDF 5.0+: Use UART_SCLK_DEFAULT
// - IDF 4.x: Use UART_SCLK_APB (APB clock source)
// - IDF 3.x: source_clk field doesn't exist in uart_config_t
#include "platforms/esp/esp_version.h"

// Only define FASTLED_UART_SCLK_SOURCE and set source_clk for IDF 4.0+
// IDF 3.x doesn't have the source_clk field in uart_config_t at all
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
#define FASTLED_UART_HAS_SOURCE_CLK 1
#define FASTLED_UART_SCLK_SOURCE UART_SCLK_DEFAULT
#elif ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 0, 0)
#define FASTLED_UART_HAS_SOURCE_CLK 1
#define FASTLED_UART_SCLK_SOURCE UART_SCLK_APB
#else
// IDF 3.x - no source_clk field exists
#define FASTLED_UART_HAS_SOURCE_CLK 0
#endif

namespace fl {

//=============================================================================
// Constructor / Destructor
//=============================================================================

UartPeripheralEsp::UartPeripheralEsp()
    : mConfig(),
      mInitialized(false),
      mResetExpireTime(0) {
}

UartPeripheralEsp::~UartPeripheralEsp() {
    // Clean up UART driver if initialized
    if (mInitialized) {
        deinitialize();
    }
}

//=============================================================================
// Lifecycle Methods
//=============================================================================

bool UartPeripheralEsp::initialize(const UartConfig& config) {
    FL_DBG("UART_PERIPH: initialize() called - uart_num=" << config.mUartNum
           << " baud=" << config.mBaudRate);

    // Validate not already initialized
    if (mInitialized) {
        FL_WARN("UartPeripheralEsp: Already initialized");
        return false;
    }

    // Store configuration
    mConfig = config;

    // Convert UART number to ESP-IDF enum
    uart_port_t uart_num = static_cast<uart_port_t>(config.mUartNum);

    // Configure UART parameters
    uart_config_t uart_config = {};
    uart_config.baud_rate = static_cast<int>(config.mBaudRate);
    uart_config.data_bits = UART_DATA_8_BITS;
    uart_config.parity = UART_PARITY_DISABLE;

    // Map stop bits (1 or 2)
    if (config.mStopBits == 1) {
        uart_config.stop_bits = UART_STOP_BITS_1;
    } else if (config.mStopBits == 2) {
        uart_config.stop_bits = UART_STOP_BITS_2;
    } else {
        FL_WARN("UartPeripheralEsp: Invalid stop bits (" << config.mStopBits
                << "), defaulting to 1");
        uart_config.stop_bits = UART_STOP_BITS_1;
    }

    uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    uart_config.rx_flow_ctrl_thresh = 0;
#if FASTLED_UART_HAS_SOURCE_CLK
    uart_config.source_clk = FASTLED_UART_SCLK_SOURCE;
#endif

    // Configure UART parameters (maps directly to ESP-IDF structure)
    FL_DBG("UART_PERIPH: Calling uart_param_config()");
    esp_err_t err = uart_param_config(uart_num, &uart_config);
    if (err != ESP_OK) {
        FL_WARN("UartPeripheralEsp: Failed to configure UART params: " << err);
        return false;
    }

    // Install UART driver with ring buffers
    // - rx_buffer_size: Typically 0 for LED control (RX not used)
    // - tx_buffer_size: Large buffer for async transmission
    // - queue_size: 0 (no event queue needed for LED control)
    // - uart_queue: NULL (no event queue)
    // - intr_alloc_flags: 0 (default interrupt priority)
    FL_DBG("UART_PERIPH: Calling uart_driver_install()");
    err = uart_driver_install(
        uart_num,
        config.mRxBufferSize,
        config.mTxBufferSize,
        0,      // queue_size
        NULL,   // uart_queue
        0       // intr_alloc_flags
    );
    if (err != ESP_OK) {
        FL_WARN("UartPeripheralEsp: Failed to install UART driver: " << err);
        return false;
    }

    // Configure GPIO pins
    // For LED control: TX-only (RX can be set to UART_PIN_NO_CHANGE or -1)
    FL_DBG("UART_PERIPH: Calling uart_set_pin()");
    int rx_pin = (config.mRxPin < 0) ? UART_PIN_NO_CHANGE : config.mRxPin;
    err = uart_set_pin(
        uart_num,
        config.mTxPin,                  // TX pin
        rx_pin,                         // RX pin (or UART_PIN_NO_CHANGE)
        UART_PIN_NO_CHANGE,             // RTS (not used)
        UART_PIN_NO_CHANGE              // CTS (not used)
    );
    if (err != ESP_OK) {
        FL_WARN("UartPeripheralEsp: Failed to set UART pins: " << err);
        // Cleanup driver on error
        uart_driver_delete(uart_num);
        return false;
    }

    mInitialized = true;
    FL_DBG("UART: Initialized (uart_num=" << config.mUartNum
           << ", baud=" << config.mBaudRate
           << ", tx_pin=" << config.mTxPin << ")");

    return true;
}

void UartPeripheralEsp::deinitialize() {
    if (!mInitialized) {
        return;
    }

    uart_port_t uart_num = static_cast<uart_port_t>(mConfig.mUartNum);

    // Wait for any pending transmissions (with timeout)
    FL_DBG("UART_PERIPH: Waiting for pending transmissions...");
    esp_err_t err = uart_wait_tx_done(uart_num, pdMS_TO_TICKS(1000));
    if (err != ESP_OK) {
        FL_WARN("UartPeripheralEsp: Wait timeout during cleanup: " << err);
    }

    // Delete UART driver
    FL_DBG("UART_PERIPH: Calling uart_driver_delete()");
    err = uart_driver_delete(uart_num);
    if (err != ESP_OK) {
        FL_WARN("UartPeripheralEsp: Failed to delete UART driver: " << err);
    }

    mInitialized = false;
    mResetExpireTime = 0;  // Clear reset period timer
    FL_DBG("UART: Deinitialized (uart_num=" << mConfig.mUartNum << ")");
}

bool UartPeripheralEsp::isInitialized() const {
    return mInitialized;
}

//=============================================================================
// Transmission Methods
//=============================================================================

bool UartPeripheralEsp::writeBytes(const uint8_t* data, size_t length) {
    if (!mInitialized) {
        FL_WARN("UartPeripheralEsp: Cannot write - not initialized");
        return false;
    }

    if (data == nullptr) {
        FL_WARN("UartPeripheralEsp: Cannot write - null buffer");
        return false;
    }

    if (length == 0) {
        // Nothing to write (not an error)
        return true;
    }

    uart_port_t uart_num = static_cast<uart_port_t>(mConfig.mUartNum);

    // Write bytes to UART TX buffer
    // - Async mode (tx_buffer_size > 0): Copies to ring buffer, returns immediately
    // - Blocking mode (tx_buffer_size = 0): Blocks until all data in FIFO
    // Returns: Number of bytes written, or -1 on error
    // Note: IDF 3.x expects const char*, IDF 4.0+ expects const void*/uint8_t*
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 0, 0)
    int written = uart_write_bytes(uart_num, data, length);
#else
    // IDF 3.x uart_write_bytes expects const char* - this cast is required for API compatibility
    int written = uart_write_bytes(uart_num, reinterpret_cast<const char*>(data), length); // ok reinterpret cast
#endif

    if (written < 0) {
        // Error occurred
        FL_WARN("UartPeripheralEsp: Failed to write bytes: " << written);
        return false;
    }

    if (static_cast<size_t>(written) != length) {
        // Partial write (shouldn't happen with blocking mode)
        FL_WARN("UartPeripheralEsp: Partial write (" << written << " of " << length << " bytes)");
        return false;
    }

    return true;
}

bool UartPeripheralEsp::waitTxDone(uint32_t timeout_ms) {
    if (!mInitialized) {
        FL_WARN("UartPeripheralEsp: Cannot wait - not initialized");
        return false;
    }

    uart_port_t uart_num = static_cast<uart_port_t>(mConfig.mUartNum);

    // Convert timeout to FreeRTOS ticks
    TickType_t timeout_ticks;
    if (timeout_ms == 0) {
        timeout_ticks = 0;  // Non-blocking poll
    } else {
        timeout_ticks = pdMS_TO_TICKS(timeout_ms);
    }

    // Wait for TX FIFO to empty
    esp_err_t err = uart_wait_tx_done(uart_num, timeout_ticks);

    // If transmission just completed, set reset timer
    if (err == ESP_OK) {
        // Calculate reset duration based on transmission time
        // For UART: transmission_time_us = (byte_count * bits_per_frame * 1000000) / baud_rate
        // Since we don't track byte count here, use a conservative estimate
        // Minimum reset period is 50us (WS2812 requirement)
        // Use a fixed reset period of 1000us (1ms) to ensure complete channel draining
        const uint64_t MIN_RESET_DURATION_US = 50;
        const uint64_t DEFAULT_RESET_DURATION_US = 1000;

        const uint64_t reset_duration = (mConfig.mBaudRate > 0) ? DEFAULT_RESET_DURATION_US : MIN_RESET_DURATION_US;
        mResetExpireTime = micros() + reset_duration;
    }

    // ESP_OK means all done, ESP_ERR_TIMEOUT means still busy (not an error)
    return (err == ESP_OK);
}

bool UartPeripheralEsp::isBusy() const {
    if (!mInitialized) {
        return false;
    }

    // Check reset period FIRST (channel draining)
    const uint64_t now = micros();
    if (now < mResetExpireTime) {
        return true;  // Still in reset period
    }

    // Then check if transmission is still in progress
    // Non-blocking poll: waitTxDone with 0 timeout
    // Cast away const since we're calling a non-const method
    return !const_cast<UartPeripheralEsp*>(this)->waitTxDone(0);
}

//=============================================================================
// State Queries
//=============================================================================

const UartConfig& UartPeripheralEsp::getConfig() const {
    return mConfig;
}

} // namespace fl

#endif // ESP32
