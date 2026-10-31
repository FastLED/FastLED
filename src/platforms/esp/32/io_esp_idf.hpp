
#pragma once

// IWYU pragma: private


// Use ESP-IDF UART + USB-Serial JTAG driver via fl::platforms wrapper
// Note: Implementation files are included via drivers/_build.hpp
#include "platforms/esp/32/detail/io_esp_jtag_or_uart.hpp"
#include "fl/stl/noexcept.h"

namespace fl {
namespace platforms {

namespace {
#if !FL_ESP_IO_USE_TINYUSB_CDC && !FL_ESP_IO_USE_USB_SERIAL_JTAG
// Output-only callers (logging, assert and driver failure diagnostics) must
// not link the SDK UART driver into sketches that never use the console
// (#4982). The native console publishes its writer the first time a caller
// that needs it (begin/read/available/flush/...) constructs it; until then
// output goes to the ROM UART, which needs no driver, ring buffers or RAM.
using ConsoleWriteFn = void (*)(const char* str, bool newline);
ConsoleWriteFn gNativeConsoleWrite = nullptr;

void nativeConsoleWrite(const char* str, bool newline) FL_NO_EXCEPT {
    EspIO& io = EspIO::instance();
    if (newline) {
        io.println(str);
    } else {
        io.print(str);
    }
}

EspIO& nativeConsole() FL_NO_EXCEPT {
    gNativeConsoleWrite = &nativeConsoleWrite;
    return EspIO::instance();
}

void romConsoleWrite(const u8* bytes, fl::size size) FL_NO_EXCEPT {
    if (fl::detail::tryConsoleWrite(bytes, size) == static_cast<fl::size>(-1)) {
        UartEsp32::writeRom(bytes, size);
    }
}

void consoleWrite(const char* str, bool newline) FL_NO_EXCEPT {
    if (!str) { return; }
    if (gNativeConsoleWrite) {
        gNativeConsoleWrite(str, newline);
        return;
    }
    romConsoleWrite(reinterpret_cast<const u8*>(str), fl::strlen(str)); // ok reinterpret cast - byte transport boundary.
    if (newline) {
        const u8 terminator = '\n';
        romConsoleWrite(&terminator, 1);
    }
}
#else
// USB consoles have no ROM UART equivalent; output needs the native driver.
EspIO& nativeConsole() FL_NO_EXCEPT {
    return EspIO::instance();
}

void consoleWrite(const char* str, bool newline) FL_NO_EXCEPT {
    if (newline) {
        EspIO::instance().println(str);
    } else {
        EspIO::instance().print(str);
    }
}
#endif
}  // namespace

// External API functions - delegation to the EspIO singleton
void begin(u32 baudRate) FL_NO_EXCEPT {
    nativeConsole().begin(baudRate);
}

void print(const char* str) FL_NO_EXCEPT {
    consoleWrite(str, false);
}

void println(const char* str) FL_NO_EXCEPT {
    consoleWrite(str, true);
}

int available() FL_NO_EXCEPT {
    return nativeConsole().available();
}

int peek() FL_NO_EXCEPT {
    return nativeConsole().peek();
}

int read() FL_NO_EXCEPT {
    return nativeConsole().read();
}

bool flush(u32 timeoutMs = 1000) FL_NO_EXCEPT {
    return nativeConsole().flush(timeoutMs);
}

size_t write_bytes(const u8* buffer, size_t size) FL_NO_EXCEPT {
    return nativeConsole().writeBytes(buffer, size);
}

bool serial_ready() FL_NO_EXCEPT {
    return nativeConsole().isReady();
}

// Test/diagnostic helper: Check if using buffered mode (not ROM UART fallback)
bool serial_is_buffered() FL_NO_EXCEPT {
    return nativeConsole().isBufferedMode();
}

int readLineNative(char delimiter, char* out, int outLen) FL_NO_EXCEPT {
    (void)delimiter; (void)out; (void)outLen;
    return -1;  // Not supported on ESP-IDF (non-Arduino) builds
}

}  // namespace platforms
}  // namespace fl
