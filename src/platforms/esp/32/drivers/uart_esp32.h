// ESP32 UART Driver - Maximum Configurability
// General-purpose UART driver for ESP-IDF, not limited to console I/O

#pragma once

#include "fl/stl/stdint.h"
#include "fl/stl/cstddef.h"

namespace fl {

// Forward declarations (no ESP-IDF headers in this file)
struct UartConfig;

/**
 * @brief UART port identifiers (platform-agnostic)
 */
enum class UartPort : u8 {
    UART0 = 0,  // Primary UART (often console)
    UART1 = 1,  // Secondary UART
    UART2 = 2   // Tertiary UART (ESP32-S3, ESP32-C3, etc.)
};

/**
 * @brief UART data bits configuration
 */
enum class UartDataBits : u8 {
    BITS_5 = 5,
    BITS_6 = 6,
    BITS_7 = 7,
    BITS_8 = 8
};

/**
 * @brief UART parity configuration
 */
enum class UartParity : u8 {
    NONE = 0,   // No parity
    ODD = 1,    // Odd parity
    EVEN = 2    // Even parity
};

/**
 * @brief UART stop bits configuration
 */
enum class UartStopBits : u8 {
    BITS_1 = 1,     // 1 stop bit
    BITS_1_5 = 2,   // 1.5 stop bits
    BITS_2 = 3      // 2 stop bits
};

/**
 * @brief UART hardware flow control configuration
 */
enum class UartFlowControl : u8 {
    NONE = 0,       // No flow control
    RTS = 1,        // RTS only
    CTS = 2,        // CTS only
    RTS_CTS = 3     // Both RTS and CTS
};

/**
 * @brief UART interrupt priority levels
 *
 * Controls ISR scheduling priority and robustness under load.
 */
enum class UartIntrPriority : u8 {
    LEVEL1 = 1,  // Lowest priority (can be masked)
    LEVEL2 = 2,  // Medium priority
    LEVEL3 = 3,  // High priority
    LEVEL4 = 4,  // Very high priority
    LEVEL5 = 5   // Highest priority (use sparingly)
};

/**
 * @brief UART interrupt flags
 *
 * Controls ISR execution characteristics:
 * - IRAM: ISR runs from IRAM, works during flash cache disable
 * - SHARED: Allow interrupt line sharing with other peripherals
 */
enum class UartIntrFlags : u8 {
    NONE = 0,          // No special flags (ISR in flash, can be blocked)
    IRAM = 1,          // ISR in IRAM (essential for reliability)
    SHARED = 2,        // Shared interrupt line
    IRAM_SHARED = 3    // Both IRAM + shared
};

/**
 * @brief UART clock source selection
 *
 * Determines which clock feeds the UART peripheral.
 * Different sources affect baud rate accuracy and power consumption.
 */
enum class UartClockSource : u8 {
    CLK_DEFAULT = 0,   // Use platform default (APB on most chips)
    CLK_APB = 1,       // APB clock (typical: 80 MHz)
    CLK_REF_TICK = 2,  // REF_TICK (1 MHz, lower power)
    CLK_XTAL = 3       // Crystal oscillator (varies by chip)
};

/**
 * @brief UART signal inversion configuration
 *
 * Allows inverting TX/RX signal levels for protocols that require inverted signals.
 */
struct UartSignalInvert {
    bool txInvert = false;   // Invert TX signal (high → low, low → high)
    bool rxInvert = false;   // Invert RX signal
    bool rtsInvert = false;  // Invert RTS signal
    bool ctsInvert = false;  // Invert CTS signal
};

/**
 * @brief RS485 half-duplex mode configuration
 *
 * Enables RS485 mode with automatic direction control via RTS pin.
 */
struct UartRS485Config {
    bool enabled = false;           // Enable RS485 mode
    bool rtsBeforeSend = false;     // Assert RTS before sending (true = active high)
    bool rtsAfterSend = true;       // Deassert RTS after sending
    u16 txWaitTime = 0;        // Wait time before/after TX (bit times)
};

/**
 * @brief UART pin configuration
 *
 * Configures GPIO pins for UART signals.
 * Use -1 for pins you don't want to configure (keeps bootloader defaults).
 */
struct UartPinConfig {
    int txPin = -1;   // TX pin (-1 = use default)
    int rxPin = -1;   // RX pin (-1 = use default)
    int rtsPin = -1;  // RTS pin (-1 = no RTS)
    int ctsPin = -1;  // CTS pin (-1 = no CTS)

    // Default constructor
    UartPinConfig() = default;

    // Constructor with all parameters
    UartPinConfig(int tx, int rx, int rts, int cts)
        : txPin(tx), rxPin(rx), rtsPin(rts), ctsPin(cts) {}

    /**
     * @brief No pin configuration (use bootloader defaults)
     */
    static UartPinConfig useDefaults() {
        return UartPinConfig{};
    }

    /**
     * @brief TX/RX only (typical for simple UART)
     */
    static UartPinConfig basic(int tx, int rx) {
        return UartPinConfig{tx, rx, -1, -1};
    }

    /**
     * @brief Full configuration with hardware flow control
     */
    static UartPinConfig withFlowControl(int tx, int rx, int rts, int cts) {
        return UartPinConfig{tx, rx, rts, cts};
    }
};

/**
 * @brief UART configuration parameters
 *
 * Comprehensive configuration for general-purpose UART usage.
 * Supports console I/O, binary protocols, RS485, and more.
 *
 * DESIGN PHILOSOPHY:
 * - No assumptions about use case (console vs data stream)
 * - Maximum configurability for all UART features
 * - Factory methods provide sensible defaults for common cases
 * - All fields have safe defaults
 */
struct UartConfig {
    // Basic UART parameters
    UartPort port = UartPort::UART0;
    u32 baudRate = 115200;
    UartDataBits dataBits = UartDataBits::BITS_8;
    UartParity parity = UartParity::NONE;
    UartStopBits stopBits = UartStopBits::BITS_1;
    UartFlowControl flowControl = UartFlowControl::NONE;

    // Buffer configuration
    size_t txBufferSize = 256;         // TX ring buffer (bytes)
    size_t rxBufferSize = 256;         // RX ring buffer (bytes)
    int rxFlowControlThresh = 0;       // RX flow control threshold (0 = disabled)

    // ISR/FreeRTOS boundary
    UartIntrPriority intrPriority = UartIntrPriority::LEVEL1;  // Default: lowest priority
    UartIntrFlags intrFlags = UartIntrFlags::NONE;             // Default: ISR in flash
    size_t eventQueueSize = 0;         // 0 = polling mode, >0 = event-driven

    // Advanced features
    UartClockSource clockSource = UartClockSource::CLK_DEFAULT;
    UartPinConfig pins = UartPinConfig::useDefaults();
    UartSignalInvert signalInvert;
    UartRS485Config rs485Config;

    // ========================================================================
    // Factory Methods - Common Configurations
    // ========================================================================

    /**
     * @brief Minimal configuration for specified port
     *
     * Uses lowest priority, ISR in flash (fragile but minimal overhead).
     * Suitable for non-critical data streams.
     */
    static UartConfig minimal(UartPort port = UartPort::UART0) {
        UartConfig config;
        config.port = port;
        return config;
    }

    /**
     * @brief High-reliability configuration (IRAM ISR, high priority)
     *
     * ISR in IRAM + LEVEL3 priority ensures UART works under all conditions:
     * - During flash operations
     * - During Wi-Fi critical sections
     * - During FreeRTOS stalls
     *
     * Recommended for: Debug UART, console I/O, critical data streams
     */
    static UartConfig reliable(UartPort port = UartPort::UART0) {
        UartConfig config;
        config.port = port;
        config.intrPriority = UartIntrPriority::LEVEL3;  // High priority
        config.intrFlags = UartIntrFlags::IRAM;          // ISR in IRAM
        return config;
    }

    /**
     * @brief High-speed data stream configuration
     *
     * Large buffers + high priority + hardware flow control.
     * Suitable for: GPS, sensors, binary protocols
     */
    static UartConfig highSpeed(UartPort port, u32 baudRate) {
        UartConfig config;
        config.port = port;
        config.baudRate = baudRate;
        config.intrPriority = UartIntrPriority::LEVEL4;  // Very high priority
        config.intrFlags = UartIntrFlags::IRAM;
        config.txBufferSize = 1024;
        config.rxBufferSize = 2048;
        config.flowControl = UartFlowControl::RTS_CTS;
        return config;
    }

    /**
     * @brief Event-driven configuration with diagnostics
     *
     * Uses FreeRTOS event queue for:
     * - Data arrival notifications
     * - Error detection (overflow, parity, framing)
     * - Break detection
     *
     * Suitable for: Modems, AT command parsers
     */
    static UartConfig eventDriven(UartPort port, size_t queueSize = 10) {
        UartConfig config;
        config.port = port;
        config.intrPriority = UartIntrPriority::LEVEL3;
        config.intrFlags = UartIntrFlags::IRAM;
        config.eventQueueSize = queueSize;  // Enable event queue
        return config;
    }

    /**
     * @brief RS485 half-duplex configuration
     *
     * Automatic direction control via RTS pin.
     * Suitable for: RS485 networks, Modbus RTU
     */
    static UartConfig rs485(UartPort port, int txPin, int rxPin, int dirPin) {
        UartConfig config;
        config.port = port;
        config.intrPriority = UartIntrPriority::LEVEL3;
        config.intrFlags = UartIntrFlags::IRAM;
        config.pins = UartPinConfig::basic(txPin, rxPin);
        config.pins.rtsPin = dirPin;  // RTS controls direction
        config.rs485Config.enabled = true;
        config.rs485Config.rtsBeforeSend = false;  // Active low (typical)
        config.rs485Config.rtsAfterSend = true;
        return config;
    }

    /**
     * @brief Custom pin configuration
     *
     * Override default pins (useful when default pins conflict with other peripherals).
     */
    static UartConfig withPins(UartPort port, int txPin, int rxPin) {
        UartConfig config;
        config.port = port;
        config.pins = UartPinConfig::basic(txPin, rxPin);
        return config;
    }
};

/**
 * @brief ESP32 UART driver with buffered I/O and fallback support
 *
 * DESIGN PHILOSOPHY:
 * - General-purpose UART driver (not console-specific)
 * - Maximum configurability via UartConfig
 * - All ESP-IDF UART features exposed through platform-agnostic API
 *
 * FEATURES:
 * - Buffered TX/RX with configurable ring buffers
 * - Optional event queue for event-driven RX
 * - Configurable ISR priority and IRAM placement
 * - RS485 half-duplex mode
 * - Signal inversion (for inverted protocols)
 * - Custom pin assignment
 * - Hardware flow control (RTS/CTS)
 *
 * INITIALIZATION:
 * - Lazy: Driver initialized in constructor
 * - Safe: Detects if Arduino or other code already installed driver
 * - Fallback: Uses ROM UART if driver installation fails
 *
 * UART ALLOCATION:
 * - Uses UartManager singleton to track allocated UARTs
 * - FL_ASSERT fires if UART already allocated
 * - Automatic deallocation on destructor
 *
 * THREAD SAFETY:
 * - ESP-IDF UART driver provides internal mutex
 * - Safe to call from multiple FreeRTOS tasks
 */
class UartEsp32 {
public:
    /**
     * @brief Construct UART driver with specified configuration
     * @param config UART configuration parameters
     *
     * Constructor performs:
     * 1. Checks UartManager to ensure UART not already allocated (FL_ASSERT if duplicate)
     * 2. Registers allocation with UartManager
     * 3. Tests if driver already installed (Arduino compatibility)
     * 4. If not installed, configures and installs driver with all settings from config
     * 5. If installation fails, falls back to ROM UART
     */
    explicit UartEsp32(const UartConfig& config);

    /**
     * @brief Destructor - deallocates UART from UartManager
     */
    ~UartEsp32();

    // Disable copy (UART is a hardware resource)
    UartEsp32(const UartEsp32&) = delete;
    UartEsp32& operator=(const UartEsp32&) = delete;

    // Enable move
    UartEsp32(UartEsp32&& other) noexcept;
    UartEsp32& operator=(UartEsp32&& other) noexcept;

    /**
     * @brief Write string to UART
     * @param str Null-terminated string to write
     *
     * Buffered mode: Copies to TX ring buffer (non-blocking)
     * Fallback mode: Writes directly to FIFO (may block if full)
     */
    void write(const char* str);

    /**
     * @brief Write raw bytes to UART (binary data)
     * @param buffer Pointer to data buffer
     * @param size Number of bytes to write
     * @return Number of bytes written
     *
     * Buffered mode: Copies to TX ring buffer (non-blocking)
     * Fallback mode: Writes directly to FIFO (may block if full)
     */
    size_t write(const u8* buffer, size_t size);

    /**
     * @brief Write string with newline to UART
     * @param str Null-terminated string to write
     */
    void writeln(const char* str);

    /**
     * @brief Check how many bytes are available to read
     * @return Number of bytes in RX ring buffer (0 if fallback mode or no data)
     *
     * Only works in buffered mode. Returns 0 in fallback mode.
     */
    int available();

    /**
     * @brief Read single byte from UART
     * @return Byte value (0-255) if data available, -1 if no data or fallback mode
     *
     * Non-blocking read (timeout=0).
     * Only works in buffered mode. Returns -1 in fallback mode.
     */
    int read();

    /**
     * @brief Flush TX buffer and wait for transmission to complete
     * @param timeoutMs Maximum time to wait (milliseconds)
     * @return true if flushed successfully, false if timeout
     *
     * Blocks until all buffered data is transmitted.
     * Only works in buffered mode.
     */
    bool flush(u32 timeoutMs = 1000);

    /**
     * @brief Get event queue handle (if configured)
     * @return Queue handle, or nullptr if no event queue configured
     *
     * Use this to receive UART events (data arrival, errors, etc.).
     * Only available if eventQueueSize > 0 in config.
     */
    void* getEventQueue() const { return mEventQueue; }

    /**
     * @brief Check if driver is in buffered mode
     * @return true if UART driver successfully installed, false if using ROM fallback
     */
    bool isBuffered() const { return mBuffered; }

    /**
     * @brief Get UART port configuration
     * @return UART port (UART0, UART1, UART2)
     */
    UartPort getPort() const { return mConfig.port; }

    /**
     * @brief Get current baud rate
     * @return Baud rate in bits per second
     */
    u32 getBaudRate() const { return mConfig.baudRate; }

private:
    UartConfig mConfig;     // Configuration parameters
    int mPortInt;           // ESP-IDF uart_port_t (stored as int to avoid including driver/uart.h)
    bool mBuffered;         // true if driver installed, false if using ROM fallback
    void* mEventQueue;      // FreeRTOS queue handle (if event queue configured)

    // Helper: Initialize UART driver (called by constructor)
    bool initDriver();
};

} // namespace fl
