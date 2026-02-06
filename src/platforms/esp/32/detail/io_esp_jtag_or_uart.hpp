// ESP32 I/O implementation - Unified UART + USB-Serial JTAG backend
// Auto-detects USB-Serial JTAG on ESP32-S3/C3/C6/H2, falls back to UART otherwise
// No Arduino dependencies - works standalone with ESP-IDF

#pragma once

#include "fl/compiler_control.h"
#include "fl/singleton.h"
#include "platforms/esp/32/drivers/uart_esp32.h"
#include "platforms/esp/32/drivers/usb_serial_jtag_esp32.h"
// Note: Implementation files are included via _build.hpp, not here

// Detect if USB-Serial JTAG is available at compile time
#if defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32C3) || \
    defined(CONFIG_IDF_TARGET_ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32H2)
#define FL_ESP_HAS_USB_SERIAL_JTAG 1
#else
#define FL_ESP_HAS_USB_SERIAL_JTAG 0
#endif

namespace fl {
namespace platforms {

/**
 * @brief ESP32 I/O manager - singleton with auto-detection of USB-Serial JTAG vs UART
 *
 * ARCHITECTURE:
 * - Singleton pattern: Constructed once on first I/O operation
 * - Runtime detection: Automatically chooses USB-Serial JTAG or UART based on chip and availability
 * - Unified API: Same interface regardless of backend
 * - No Arduino dependencies: Works standalone with ESP-IDF
 *
 * INITIALIZATION:
 * - Lazy: Driver is initialized on first I/O operation
 * - Thread-safe: Singleton pattern ensures single initialization
 *
 * BACKEND SELECTION:
 * - ESP32-S3/C3/C6/H2: Try USB-Serial JTAG first, fall back to UART0 if installation fails
 * - Other ESP32 chips: Use UART0 directly
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

    // Initialize/reconfigure serial
    void begin(uint32_t baudRate) {
        // Both drivers are already initialized in constructor with default baud rate
        // USB-Serial JTAG doesn't support baud rate configuration (fixed USB speed)
        // UART reconfiguration after initialization is not currently supported
        // Future enhancement: Add UartEsp32::setBaudRate() method
        (void)baudRate;
    }

    // Print string to serial
    void print(const char* str) {
#if FL_ESP_HAS_USB_SERIAL_JTAG
        if (mUseUsbSerialJtag) {
            mUsbSerialJtag.write(str);
        } else {
            mUart.write(str);
        }
#else
        mUart.write(str);
#endif
    }

    // Print string with newline to serial
    void println(const char* str) {
#if FL_ESP_HAS_USB_SERIAL_JTAG
        if (mUseUsbSerialJtag) {
            mUsbSerialJtag.writeln(str);
        } else {
            mUart.writeln(str);
        }
#else
        mUart.writeln(str);
#endif
    }

    // Check input availability
    int available() {
#if FL_ESP_HAS_USB_SERIAL_JTAG
        if (mUseUsbSerialJtag) {
            return mUsbSerialJtag.available();
        } else {
            return mUart.available();
        }
#else
        return mUart.available();
#endif
    }

    // Peek at next character without removing it
    int peek() {
        if (mHasPeek) {
            return mPeekByte;
        }

        // Read next byte and cache it
#if FL_ESP_HAS_USB_SERIAL_JTAG
        mPeekByte = mUseUsbSerialJtag ? mUsbSerialJtag.read() : mUart.read();
#else
        mPeekByte = mUart.read();
#endif

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

#if FL_ESP_HAS_USB_SERIAL_JTAG
        return mUseUsbSerialJtag ? mUsbSerialJtag.read() : mUart.read();
#else
        return mUart.read();
#endif
    }

    // Write raw bytes to serial (binary data)
    size_t writeBytes(const uint8_t* buffer, size_t size) {
#if FL_ESP_HAS_USB_SERIAL_JTAG
        if (mUseUsbSerialJtag) {
            return mUsbSerialJtag.write(buffer, size);
        } else {
            return mUart.write(buffer, size);
        }
#else
        return mUart.write(buffer, size);
#endif
    }

    // Flush TX buffer and wait for transmission to complete
    bool flush(uint32_t timeoutMs = 1000) {
#if FL_ESP_HAS_USB_SERIAL_JTAG
        if (mUseUsbSerialJtag) {
            return mUsbSerialJtag.flush(timeoutMs);
        } else {
            return mUart.flush(timeoutMs);
        }
#else
        return mUart.flush(timeoutMs);
#endif
    }

    // Check if serial is ready
    bool isReady() const {
#if FL_ESP_HAS_USB_SERIAL_JTAG
        return mUseUsbSerialJtag ? mUsbSerialJtag.isBuffered() : mUart.isBuffered();
#else
        return mUart.isBuffered();
#endif
    }

    // Check if using buffered mode (for testing/diagnostics)
    bool isBufferedMode() const {
#if FL_ESP_HAS_USB_SERIAL_JTAG
        return mUseUsbSerialJtag ? mUsbSerialJtag.isBuffered() : mUart.isBuffered();
#else
        return mUart.isBuffered();
#endif
    }

    // Get reference to underlying UART driver (for advanced use)
    // Note: May throw if USB-Serial JTAG is active instead
    UartEsp32& getUart() {
#if FL_ESP_HAS_USB_SERIAL_JTAG
        FL_ASSERT(!mUseUsbSerialJtag, "Cannot get UART driver - using USB-Serial JTAG instead");
#endif
        return mUart;
    }

#if FL_ESP_HAS_USB_SERIAL_JTAG
    // Get reference to USB-Serial JTAG driver (for advanced use)
    // Note: May throw if UART is active instead
    UsbSerialJtagEsp32& getUsbSerialJtag() {
        FL_ASSERT(mUseUsbSerialJtag, "Cannot get USB-Serial JTAG driver - using UART instead");
        return mUsbSerialJtag;
    }

    // Check if currently using USB-Serial JTAG
    bool isUsingUsbSerialJtag() const {
        return mUseUsbSerialJtag;
    }
#endif

private:
#if FL_ESP_HAS_USB_SERIAL_JTAG
    UsbSerialJtagEsp32 mUsbSerialJtag;  // USB-Serial JTAG driver (ESP32-S3/C3/C6/H2 only)
    bool mUseUsbSerialJtag;             // true if USB-Serial JTAG active
#endif
    UartEsp32 mUart;       // UART driver (UART0 with console-specific configuration)
    int mPeekByte;         // Cached peek byte
    bool mHasPeek;         // True if peek byte is valid

    // Constructor: Initialize drivers with runtime detection
    EspIO()
        :
#if FL_ESP_HAS_USB_SERIAL_JTAG
          mUsbSerialJtag(UsbSerialJtagConfig::defaults()),
          mUseUsbSerialJtag(false),  // Start false, detect below
#endif
          mUart(UartConfig::reliable(UartPort::UART0)),
          mPeekByte(-1),
          mHasPeek(false) {

#if FL_ESP_HAS_USB_SERIAL_JTAG
        // Runtime detection: Prefer USB-Serial JTAG if available and working
        // The drivers already print their status during initialization

        if (mUsbSerialJtag.isBuffered()) {
            // Our USB-Serial JTAG driver installed successfully
            mUseUsbSerialJtag = true;
            esp_rom_printf("EspIO: Using ESP-IDF USB-Serial JTAG driver\n");
        } else {
            // USB-Serial JTAG driver failed to install - fall back to UART0
            mUseUsbSerialJtag = false;
            esp_rom_printf("EspIO: USB-Serial JTAG installation failed - falling back to UART0\n");
        }
#endif
    }

    friend class fl::Singleton<EspIO>;
};


} // namespace platforms
} // namespace fl
