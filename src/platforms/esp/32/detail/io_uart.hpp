// ESP32 I/O implementation - ESP-IDF UART driver backend
// Uses centralized UartEsp32 driver for buffered serial I/O

#pragma once

#include "fl/compiler_control.h"
#include "fl/singleton.h"
#include "platforms/esp/32/drivers/uart_esp32.h"
// Note: uart_esp32.cpp.hpp is included via _build.hpp, not here

namespace fl {
namespace platforms {

/**
 * @brief ESP32 I/O manager - singleton that delegates to UartEsp32 driver
 *
 * ARCHITECTURE:
 * - Singleton pattern: Constructed once on first I/O operation
 * - Delegation: All I/O operations delegate to UartEsp32 driver
 * - UART0: Uses console UART (same as Serial on Arduino)
 *
 * INITIALIZATION:
 * - Lazy: UartEsp32 driver is initialized on first I/O operation
 * - Thread-safe: Singleton pattern ensures single initialization
 *
 * USAGE:
 * - Do not construct directly - use print(), println(), available(), read()
 * - These functions call EspIO::instance() which returns the singleton
 */
class EspIO {
public:
    // Get singleton instance
    static EspIO& instance() {
        return fl::Singleton<EspIO>::instance();
    }

    // Initialize/reconfigure UART
    void begin(uint32_t baudRate) {
        // UART is already initialized in constructor with default baud rate (115200)
        // Reconfiguring baud rate after initialization is not currently supported
        // Future enhancement: Add UartEsp32::setBaudRate() method
        (void)baudRate;
    }

    // Print string to UART
    void print(const char* str) {
        mUart.write(str);
    }

    // Print string with newline to UART
    void println(const char* str) {
        mUart.writeln(str);
    }

    // Check input availability
    int available() {
        return mUart.available();
    }

    // Peek at next character without removing it
    int peek() {
        if (mHasPeek) {
            return mPeekByte;
        }

        // Read next byte and cache it
        mPeekByte = mUart.read();
        if (mPeekByte != -1) {
            mHasPeek = true;
        }
        return mPeekByte;
    }

    // Read single character
    int read() {
        // Return peeked byte if available
        if (mHasPeek) {
            mHasPeek = false;
            return mPeekByte;
        }
        return mUart.read();
    }

    // Write raw bytes to UART (binary data)
    size_t writeBytes(const uint8_t* buffer, size_t size) {
        return mUart.write(buffer, size);
    }

    // Flush TX buffer and wait for transmission to complete
    bool flush(uint32_t timeoutMs = 1000) {
        return mUart.flush(timeoutMs);
    }

    // Check if serial is ready
    bool isReady() const {
        return mUart.isBuffered();  // Ready if UART driver installed successfully
    }

    // Get reference to underlying UART driver (for advanced use)
    UartEsp32& getUart() {
        return mUart;
    }

private:
    UartEsp32 mUart;  // UART driver (UART0 with console-specific configuration)
    int mPeekByte;    // Cached peek byte
    bool mHasPeek;    // True if peek byte is valid

    // Constructor: Initialize UART driver with console-specific configuration
    // Console config: LEVEL3 priority + IRAM ISR = debug prints always work
    EspIO() : mUart(UartConfig::reliable(UartPort::UART0)), mPeekByte(-1), mHasPeek(false) {
        // UartEsp32 constructor handles all initialization
        // Uses reliable() factory: high-priority IRAM ISR for robust debug output
    }

    friend class fl::Singleton<EspIO>;
};

// External API functions - simple delegation to EspIO singleton
void begin(uint32_t baudRate) {
    EspIO::instance().begin(baudRate);
}

void print(const char* str) {
    EspIO::instance().print(str);
}

void println(const char* str) {
    EspIO::instance().println(str);
}

int available() {
    return EspIO::instance().available();
}

int peek() {
    return EspIO::instance().peek();
}

int read() {
    return EspIO::instance().read();
}

bool flush(uint32_t timeoutMs = 1000) {
    return EspIO::instance().flush(timeoutMs);
}

size_t write_bytes(const uint8_t* buffer, size_t size) {
    return EspIO::instance().writeBytes(buffer, size);
}

bool serial_ready() {
    return EspIO::instance().isReady();
}

} // namespace platforms
} // namespace fl
