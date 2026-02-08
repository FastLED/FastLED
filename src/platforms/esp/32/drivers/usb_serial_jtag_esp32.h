// ESP32 USB-Serial JTAG Driver - ESP32-S3/C3/C6/H2 Support
// Provides buffered serial I/O via USB-Serial JTAG peripheral

#pragma once

#include "fl/stl/stdint.h"
#include "fl/stl/cstddef.h"

namespace fl {

/**
 * @brief USB-Serial JTAG configuration parameters
 *
 * Simplified configuration compared to UART - USB-Serial JTAG has fewer knobs.
 */
struct UsbSerialJtagConfig {
    size_t txBufferSize = 4096;   // TX ring buffer (bytes)
    size_t rxBufferSize = 4096;   // RX ring buffer (bytes)

    /**
     * @brief Default configuration (4096-byte buffers)
     */
    static UsbSerialJtagConfig defaults() {
        return UsbSerialJtagConfig{};
    }

    /**
     * @brief High-throughput configuration (larger buffers)
     */
    static UsbSerialJtagConfig highThroughput() {
        UsbSerialJtagConfig config;
        config.txBufferSize = 4096*4;
        config.rxBufferSize = 4096*4;
        return config;
    }
};

/**
 * @brief ESP32 USB-Serial JTAG driver with buffered I/O
 *
 * DESIGN PHILOSOPHY:
 * - Drop-in replacement for UartEsp32 when USB-Serial JTAG is available
 * - Matches UartEsp32 API surface for easy swapping
 * - Automatic detection of USB connection status
 *
 * FEATURES:
 * - Buffered TX/RX with configurable ring buffers
 * - Non-blocking writes with automatic buffering
 * - Connection detection (usb_serial_jtag_is_connected)
 * - Graceful fallback to ROM UART if driver unavailable
 *
 * PLATFORM SUPPORT:
 * - ESP32-S3: USB-Serial JTAG on pins 19 (D-) and 20 (D+)
 * - ESP32-C3: USB-Serial JTAG built-in
 * - ESP32-C6: USB-Serial JTAG built-in
 * - ESP32-H2: USB-Serial JTAG built-in
 * - Other chips: Not supported (will fall back to ROM UART)
 *
 * INITIALIZATION:
 * - Lazy: Driver initialized in constructor
 * - Safe: Detects if driver already installed
 * - Fallback: Uses ROM UART if USB-Serial JTAG unavailable
 */
class UsbSerialJtagEsp32 {
public:
    /**
     * @brief Construct USB-Serial JTAG driver with specified configuration
     * @param config USB-Serial JTAG configuration parameters
     *
     * Constructor performs:
     * 1. Checks if USB-Serial JTAG is available on this chip
     * 2. Tests if driver already installed
     * 3. If not installed, installs driver with buffer sizes from config
     * 4. If installation fails, falls back to ROM UART
     */
    explicit UsbSerialJtagEsp32(const UsbSerialJtagConfig& config);

    /**
     * @brief Destructor - uninstalls driver if we installed it
     */
    ~UsbSerialJtagEsp32();

    // Disable copy (USB-Serial JTAG is a hardware resource)
    UsbSerialJtagEsp32(const UsbSerialJtagEsp32&) = delete;
    UsbSerialJtagEsp32& operator=(const UsbSerialJtagEsp32&) = delete;

    // Enable move
    UsbSerialJtagEsp32(UsbSerialJtagEsp32&& other) noexcept;
    UsbSerialJtagEsp32& operator=(UsbSerialJtagEsp32&& other) noexcept;

    /**
     * @brief Write string to USB-Serial JTAG
     * @param str Null-terminated string to write
     *
     * Buffered mode: Copies to TX ring buffer (non-blocking)
     * Fallback mode: Writes directly via ROM UART (may block if full)
     */
    void write(const char* str);

    /**
     * @brief Write raw bytes to USB-Serial JTAG (binary data)
     * @param buffer Pointer to data buffer
     * @param size Number of bytes to write
     * @return Number of bytes written
     *
     * Buffered mode: Copies to TX ring buffer (non-blocking)
     * Fallback mode: Writes directly via ROM UART (may block if full)
     */
    size_t write(const u8* buffer, size_t size);

    /**
     * @brief Write string with newline to USB-Serial JTAG
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
     * @brief Read single byte from USB-Serial JTAG
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
     * @brief Check if USB-Serial JTAG is connected to host
     * @return true if USB cable connected and host enumerated device
     *
     * Uses usb_serial_jtag_is_connected() ESP-IDF function.
     * Returns false if driver not installed.
     */
    bool isConnected() const;

    /**
     * @brief Check if driver is in buffered mode
     * @return true if driver successfully installed, false if using ROM fallback
     */
    bool isBuffered() const { return mBuffered; }

private:
    UsbSerialJtagConfig mConfig;  // Configuration parameters
    bool mBuffered;               // true if driver installed, false if using ROM fallback
    bool mInstalledDriver;        // true if WE installed the driver (vs inherited from Arduino)

    // Helper: Initialize USB-Serial JTAG driver (called by constructor)
    bool initDriver();
};

} // namespace fl
