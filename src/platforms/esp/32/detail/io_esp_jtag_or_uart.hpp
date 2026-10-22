// ESP32 I/O implementation with one selected native console backend.

#pragma once

// IWYU pragma: private

#include "platforms/esp/is_esp.h"

#include "fl/log/log.h"  // FL_PRINT (used by reportInitDiagnosticsIfNeeded)
#include "fl/stl/compiler_control.h"
#include "fl/stl/singleton.h"
#include "fl/stl/vector.h"
#include "fl/stl/cstring.h"
#include "platforms/esp/32/detail/console_write.h"
#include "platforms/esp/32/drivers/uart_esp32.h"
#include "platforms/esp/32/drivers/usb_serial_jtag_esp32.h"
#include "platforms/esp/32/drivers/usb_cdc_esp32.h"
#include "fl/stl/noexcept.h"
// Note: Implementation files are included via _build.hpp, not here

#include "platforms/esp/32/detail/console_config.h"

namespace fl {
namespace platforms {

/**
 * @brief ESP32 I/O manager with compile-time native transport selection
 *
 * ARCHITECTURE:
 * - Singleton pattern: Constructed once on first I/O operation
 * - Compile-time selection: native UART, hardware USB-JTAG or TinyUSB CDC
 * - Unified API: Same interface regardless of backend
 * - No Arduino dependencies: Works standalone with ESP-IDF
 *
 * INITIALIZATION:
 * - Lazy: Driver is initialized on first I/O operation
 * - Thread-safe: Singleton pattern ensures single initialization
 *
 * BACKEND SELECTION:
 * - Board configuration selects one native backend.
 * - TinyUSB CDC requires explicit device-stack ownership at build time.
 *
 * USAGE:
 * - Do not construct directly - use print(), println(), available(), read()
 * - These functions call EspIO::instance() which returns the singleton
 */
class EspIO {
public:
    // Get singleton instance
    static EspIO& instance() FL_NO_EXCEPT {
        return fl::Singleton<EspIO>::instance();
    }

    // Initialize/reconfigure serial
    void begin(u32 baudRate) FL_NO_EXCEPT {
        mDriver.setBaudRate(baudRate);
    }

    // Print string to serial
    void print(const char* str) FL_NO_EXCEPT {
        reportInitDiagnosticsIfNeeded();
        if (!str) { return; }
        writeBytes(reinterpret_cast<const u8*>(str), fl::strlen(str)); // ok reinterpret cast - byte transport boundary.
    }

    // Print string with newline to serial
    void println(const char* str) FL_NO_EXCEPT {
        reportInitDiagnosticsIfNeeded();
        if (!str) { return; }
        const fl::size length = fl::strlen(str);
        if (length == static_cast<fl::size>(-1)) { return; }
        vector_inlined<u8, 128> line;
        line.resize(length + 1);
        if (line.size() != length + 1) { return; }
        fl::memcpy(line.data(), str, length);
        line[length] = '\n';
        // The shared SDK UART TX mutex covers this complete buffer, including
        // its terminator, even when another task uses Arduino Serial.write().
        writeBytes(line.data(), line.size());
    }

    // Check input availability
    int available() FL_NO_EXCEPT {
        return mDriver.available() + (mHasPeek ? 1 : 0);
    }

    // Peek at next character without removing it
    int peek() FL_NO_EXCEPT {
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
    int read() FL_NO_EXCEPT {
        if (mHasPeek) {
            mHasPeek = false;
            return mPeekByte;
        }
        return mDriver.read();
    }

    // Write raw bytes to serial (binary data)
    size_t writeBytes(const u8* buffer, size_t size) FL_NO_EXCEPT {
        const fl::size overridden = fl::detail::tryConsoleWrite(buffer, size);
        if (overridden != static_cast<fl::size>(-1)) { return overridden; }
        return mDriver.write(buffer, size);
    }

    // Flush TX buffer and wait for transmission to complete
    bool flush(u32 timeoutMs = 1000) FL_NO_EXCEPT {
        return mDriver.flush(timeoutMs);
    }

    // Check if serial is ready
    bool isReady() const FL_NO_EXCEPT {
        return mDriver.isReady();
    }

    // Check if using buffered mode (for testing/diagnostics)
    bool isBufferedMode() const FL_NO_EXCEPT {
        return mDriver.isBuffered();
    }

    // Compile-time: which backend is the active one in this build?
    static constexpr bool isUsingUsbSerialJtag() FL_NO_EXCEPT {
        return FL_ESP_IO_USE_USB_SERIAL_JTAG;
    }

#if FL_ESP_IO_USE_TINYUSB_CDC
    UsbCdcEsp32& getUsbCdc() FL_NO_EXCEPT { return mDriver; }
#elif FL_ESP_IO_USE_USB_SERIAL_JTAG
    // Get reference to USB-Serial JTAG driver (for advanced use).
    // Available only when this build was configured with the JTAG backend.
    UsbSerialJtagEsp32& getUsbSerialJtag() FL_NO_EXCEPT {
        return mDriver;
    }
#else
    // Get reference to underlying UART driver (for advanced use).
    // Available only when this build was configured with the UART backend.
    UartEsp32& getUart() FL_NO_EXCEPT {
        return mDriver;
    }
#endif

private:
#if FL_ESP_IO_USE_TINYUSB_CDC
    using ActiveDriver = UsbCdcEsp32;
#elif FL_ESP_IO_USE_USB_SERIAL_JTAG
    using ActiveDriver = UsbSerialJtagEsp32;
#else
    using ActiveDriver = UartEsp32;
#endif

    ActiveDriver mDriver;
    int mPeekByte;
    bool mHasPeek;
    bool mDiagnosticsReported;

    EspIO() FL_NO_EXCEPT
        : mDriver(), mPeekByte(-1), mHasPeek(false),
          mDiagnosticsReported(false) {}

    // Emit a one-shot human-readable summary of the compile-time backend
    // choice on the first user-facing print/println.
    void reportInitDiagnosticsIfNeeded() FL_NO_EXCEPT {
        if (mDiagnosticsReported) {
            return;
        }
        mDiagnosticsReported = true;
#if FL_ESP_IO_USE_TINYUSB_CDC
        FL_PRINT("EspIO: backend=USB-CDC (native TinyUSB)");
#elif FL_ESP_IO_USE_USB_SERIAL_JTAG
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
        FL_PRINT("EspIO: backend=" << backend << " usb_jtag_outcome=" << outcome << " err=" << (static_cast<int>(mDriver.initError())));
#else
        FL_PRINT("EspIO: backend=UART0 (compile-time choice; "
                 "#define FASTLED_ESP_FORCE_USB_SERIAL_JTAG to switch)");
#endif
    }

    friend class fl::Singleton<EspIO>;
};


} // namespace platforms
} // namespace fl
