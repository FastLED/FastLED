// ESP32 I/O implementation - Unified UART + USB-Serial JTAG backend
// Auto-detects USB-Serial JTAG on ESP32-S3/C3/C6/H2, falls back to UART otherwise
// No Arduino dependencies - works standalone with ESP-IDF

#pragma once

// IWYU pragma: private

#include "platforms/esp/is_esp.h"

#include "fl/log/log.h"  // FL_PRINT (used by reportInitDiagnosticsIfNeeded)
#include "fl/stl/compiler_control.h"
#include "fl/stl/singleton.h"
#include "platforms/esp/32/drivers/uart_esp32.h"
#include "platforms/esp/32/drivers/usb_serial_jtag_esp32.h"
#include "fl/stl/noexcept.h"
// Note: Implementation files are included via _build.hpp, not here

// Detect if USB-Serial JTAG is available at compile time
#if defined(FL_IS_ESP_32S3) || defined(FL_IS_ESP_32C3) || \
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
    static EspIO& instance() FL_NOEXCEPT {
        return fl::Singleton<EspIO>::instance();
    }

    // Initialize/reconfigure serial
    void begin(u32 baudRate) FL_NOEXCEPT {
        // Both drivers are already initialized in constructor with default baud rate
        // USB-Serial JTAG doesn't support baud rate configuration (fixed USB speed)
        // UART reconfiguration after initialization is not currently supported
        // Future enhancement: Add UartEsp32::setBaudRate() method
        (void)baudRate;
    }

    // Print string to serial
    void print(const char* str) FL_NOEXCEPT {
        reportInitDiagnosticsIfNeeded();
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
    void println(const char* str) FL_NOEXCEPT {
        reportInitDiagnosticsIfNeeded();
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
    int available() FL_NOEXCEPT {
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
    int peek() FL_NOEXCEPT {
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
    int read() FL_NOEXCEPT {
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
    size_t writeBytes(const u8* buffer, size_t size) FL_NOEXCEPT {
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
    bool flush(u32 timeoutMs = 1000) FL_NOEXCEPT {
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
    bool isReady() const FL_NOEXCEPT {
#if FL_ESP_HAS_USB_SERIAL_JTAG
        return mUseUsbSerialJtag ? mUsbSerialJtag.isBuffered() : mUart.isBuffered();
#else
        return mUart.isBuffered();
#endif
    }

    // Check if using buffered mode (for testing/diagnostics)
    bool isBufferedMode() const FL_NOEXCEPT {
#if FL_ESP_HAS_USB_SERIAL_JTAG
        return mUseUsbSerialJtag ? mUsbSerialJtag.isBuffered() : mUart.isBuffered();
#else
        return mUart.isBuffered();
#endif
    }

    // Get reference to underlying UART driver (for advanced use)
    // Note: May throw if USB-Serial JTAG is active instead
    UartEsp32& getUart() FL_NOEXCEPT {
#if FL_ESP_HAS_USB_SERIAL_JTAG
        FL_ASSERT(!mUseUsbSerialJtag, "Cannot get UART driver - using USB-Serial JTAG instead");
#endif
        return mUart;
    }

#if FL_ESP_HAS_USB_SERIAL_JTAG
    // Get reference to USB-Serial JTAG driver (for advanced use)
    // Note: May throw if UART is active instead
    UsbSerialJtagEsp32& getUsbSerialJtag() FL_NOEXCEPT {
        FL_ASSERT(mUseUsbSerialJtag, "Cannot get USB-Serial JTAG driver - using UART instead");
        return mUsbSerialJtag;
    }

    // Check if currently using USB-Serial JTAG
    bool isUsingUsbSerialJtag() const FL_NOEXCEPT {
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
    bool mDiagnosticsReported;  // true once the one-shot init diagnostic ran

    // Constructor: Initialize drivers with runtime detection
    EspIO()
        :
#if FL_ESP_HAS_USB_SERIAL_JTAG
          mUsbSerialJtag(UsbSerialJtagConfig::defaults()),
          mUseUsbSerialJtag(false),  // Start false, detect below
#endif
          mUart(UartConfig::reliable(UartPort::UART0)),
          mPeekByte(-1),
          mHasPeek(false),
          mDiagnosticsReported(false) {

#if FL_ESP_HAS_USB_SERIAL_JTAG
        // Runtime detection: Prefer USB-Serial JTAG if available and working.
        // The chosen backend and the JTAG init outcome are surfaced via
        // reportInitDiagnosticsIfNeeded() on the first user-facing print —
        // FL_PRINT here would re-enter the in-progress singleton.
        mUseUsbSerialJtag = mUsbSerialJtag.isBuffered();
#endif
    }

    // Emit a one-shot human-readable summary of the boot-time backend choice
    // and the USB-Serial JTAG init outcome. Called from the first user-facing
    // print/println — guaranteed to run after EspIO construction completes,
    // so FL_PRINT (which dispatches back through this singleton) is safe.
    void reportInitDiagnosticsIfNeeded() FL_NOEXCEPT {
        if (mDiagnosticsReported) {
            return;
        }
        mDiagnosticsReported = true;
#if FL_ESP_HAS_USB_SERIAL_JTAG
        const char* backend = mUseUsbSerialJtag ? "USB-JTAG" : "UART0";
        const char* outcome = nullptr;
        switch (mUsbSerialJtag.initOutcome()) {
            case UsbSerialJtagEsp32::InitOutcome::kNotInitialized:      outcome = "NotInitialized"; break;
            case UsbSerialJtagEsp32::InitOutcome::kPreInstalled:        outcome = "PreInstalled"; break;
            case UsbSerialJtagEsp32::InitOutcome::kInstalledOk:         outcome = "InstalledOk"; break;
            case UsbSerialJtagEsp32::InitOutcome::kInstalledUnverified: outcome = "InstalledUnverified"; break;
            case UsbSerialJtagEsp32::InitOutcome::kInstallFailed:       outcome = "InstallFailed"; break;
            case UsbSerialJtagEsp32::InitOutcome::kVerificationFailed:  outcome = "VerificationFailed"; break;
            case UsbSerialJtagEsp32::InitOutcome::kNotAvailable:        outcome = "NotAvailable"; break;
        }
        FL_PRINT("EspIO: backend=" << backend
                 << " usb_jtag_outcome=" << outcome
                 << " err=" << static_cast<int>(mUsbSerialJtag.initError()));
#else
        FL_PRINT("EspIO: backend=UART0 (USB-Serial JTAG not compiled)");
#endif
    }

    friend class fl::Singleton<EspIO>;
};


} // namespace platforms
} // namespace fl
