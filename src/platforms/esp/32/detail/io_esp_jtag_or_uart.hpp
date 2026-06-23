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

// Compile-time selection of the active console backend.
//
// Previously EspIO held BOTH UartEsp32 AND UsbSerialJtagEsp32 as direct
// members and runtime-probed which one was buffered. The linker had to
// keep both drivers (and the ESP-IDF JTAG driver they pull in) alive
// because both were reachable from every print/read/write/flush path —
// ~2-3 KB of dead weight when only one backend is actually used. See
// FastLED #2773 item 1.4.
//
// New rule: pick ONE backend at compile time. The unused driver is
// dead code and gets dropped by `--gc-sections`. The choice mirrors
// Arduino-ESP32's existing `ARDUINO_USB_CDC_ON_BOOT` convention:
//
//   * ARDUINO_USB_CDC_ON_BOOT=1 (Arduino routes Serial through USB
//     Serial-JTAG)     → EspIO uses UsbSerialJtagEsp32.
//   * otherwise (Arduino-default UART0 console, or non-Arduino ESP-IDF
//     build)            → EspIO uses UartEsp32.
//
// Explicit overrides for the rare case where a user wants a different
// backend than what their Arduino board config implies:
//
//   * `#define FASTLED_ESP_FORCE_UART_SERIAL`        → forces UART.
//   * `#define FASTLED_ESP_FORCE_USB_SERIAL_JTAG`    → forces JTAG.
//
// Both macros are honored only when JTAG hardware is available; on
// chips without JTAG (`FL_ESP_HAS_USB_SERIAL_JTAG == 0`) the choice is
// fixed to UART.
#if !FL_ESP_HAS_USB_SERIAL_JTAG
#define FL_ESP_IO_USE_USB_SERIAL_JTAG 0
#elif defined(FASTLED_ESP_FORCE_UART_SERIAL)
#define FL_ESP_IO_USE_USB_SERIAL_JTAG 0
#elif defined(FASTLED_ESP_FORCE_USB_SERIAL_JTAG)
#define FL_ESP_IO_USE_USB_SERIAL_JTAG 1
#elif defined(ARDUINO_USB_CDC_ON_BOOT) && ARDUINO_USB_CDC_ON_BOOT
#define FL_ESP_IO_USE_USB_SERIAL_JTAG 1
#else
#define FL_ESP_IO_USE_USB_SERIAL_JTAG 0
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
        mDriver.write(str);
    }

    // Print string with newline to serial
    void println(const char* str) FL_NOEXCEPT {
        reportInitDiagnosticsIfNeeded();
        mDriver.writeln(str);
    }

    // Check input availability
    int available() FL_NOEXCEPT {
        return mDriver.available();
    }

    // Peek at next character without removing it
    int peek() FL_NOEXCEPT {
        if (mHasPeek) {
            return mPeekByte;
        }
        mPeekByte = mDriver.read();
        if (mPeekByte != -1) {
            mHasPeek = true;
        }
        return mPeekByte;
    }

    // Read single character
    int read() FL_NOEXCEPT {
        if (mHasPeek) {
            mHasPeek = false;
            return mPeekByte;
        }
        return mDriver.read();
    }

    // Write raw bytes to serial (binary data)
    size_t writeBytes(const u8* buffer, size_t size) FL_NOEXCEPT {
        return mDriver.write(buffer, size);
    }

    // Flush TX buffer and wait for transmission to complete
    bool flush(u32 timeoutMs = 1000) FL_NOEXCEPT {
        return mDriver.flush(timeoutMs);
    }

    // Check if serial is ready
    bool isReady() const FL_NOEXCEPT {
        return mDriver.isBuffered();
    }

    // Check if using buffered mode (for testing/diagnostics)
    bool isBufferedMode() const FL_NOEXCEPT {
        return mDriver.isBuffered();
    }

    // Compile-time: which backend is the active one in this build?
    static constexpr bool isUsingUsbSerialJtag() FL_NOEXCEPT {
        return FL_ESP_IO_USE_USB_SERIAL_JTAG;
    }

#if FL_ESP_IO_USE_USB_SERIAL_JTAG
    // Get reference to USB-Serial JTAG driver (for advanced use).
    // Available only when this build was configured with the JTAG backend.
    UsbSerialJtagEsp32& getUsbSerialJtag() FL_NOEXCEPT {
        return mDriver;
    }
#else
    // Get reference to underlying UART driver (for advanced use).
    // Available only when this build was configured with the UART backend.
    UartEsp32& getUart() FL_NOEXCEPT {
        return mDriver;
    }
#endif

private:
#if FL_ESP_IO_USE_USB_SERIAL_JTAG
    // Single driver member chosen at compile time. The OTHER driver's
    // code is dead and dropped by `--gc-sections`. See the comment block
    // at the top of this file (#2773 item 1.4).
    using ActiveDriver = UsbSerialJtagEsp32;
    static UsbSerialJtagConfig defaultConfig() FL_NOEXCEPT {
        return UsbSerialJtagConfig::defaults();
    }
#else
    using ActiveDriver = UartEsp32;
    static UartConfig defaultConfig() FL_NOEXCEPT {
        return UartConfig::reliable(UartPort::UART0);
    }
#endif

    ActiveDriver mDriver;            // The one backend selected at compile time.
    int mPeekByte;                   // Cached peek byte
    bool mHasPeek;                   // True if peek byte is valid
    bool mDiagnosticsReported;       // true once the one-shot init diagnostic ran

    // Constructor: initialize the single chosen driver.
    EspIO()
        : mDriver(defaultConfig()),
          mPeekByte(-1),
          mHasPeek(false),
          mDiagnosticsReported(false) {}

    // Emit a one-shot human-readable summary of the compile-time backend
    // choice on the first user-facing print/println.
    void reportInitDiagnosticsIfNeeded() FL_NOEXCEPT {
        if (mDiagnosticsReported) {
            return;
        }
        mDiagnosticsReported = true;
#if FL_ESP_IO_USE_USB_SERIAL_JTAG
        const char* backend = "USB-JTAG";
        const char* outcome = nullptr;
        switch (mDriver.initOutcome()) {
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
                 << " err=" << static_cast<int>(mDriver.initError()));
#else
        FL_PRINT("EspIO: backend=UART0 (compile-time choice; "
                 "#define FASTLED_ESP_FORCE_USB_SERIAL_JTAG to switch)");
#endif
    }

    friend class fl::Singleton<EspIO>;
};


} // namespace platforms
} // namespace fl
