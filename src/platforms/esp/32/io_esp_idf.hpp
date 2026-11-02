
#pragma once

// IWYU pragma: private


// Use ESP-IDF UART + USB-Serial JTAG driver via fl::platforms wrapper
// Note: Implementation files are included via drivers/_build.hpp
#include "platforms/esp/32/detail/io_esp_jtag_or_uart.hpp"
#include "fl/stl/atomic.h"
#include "freertos/FreeRTOS.h"  // IWYU pragma: keep
#include "freertos/task.h"  // IWYU pragma: keep
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
// Switch hand-off without a mutex (a lock costs ~900 B flash on esp32c3):
// writers count themselves in-flight around ROM writes; the switching task
// waits for that count to drain before the driver install resets UART0.
using ConsoleWriteFn = void (*)(const char* str, bool newline);
// Only nativeConsole() takes the address, so output-only builds drop EspIO.
ConsoleWriteFn gNativeConsoleWrite = nullptr;
// One word: kReady | kSwitching | in-flight ROM writer count.
constexpr fl::u32 kReady = 1u << 31;
constexpr fl::u32 kSwitching = 1u << 30;
fl::atomic_u32 gConsoleState(0);

// Sets `bits`, returning the previous state (fl::atomic has no fetch_or).
fl::u32 setConsoleBits(fl::u32 bits) FL_NO_EXCEPT {
    fl::u32 prev = gConsoleState.load();
    while (!gConsoleState.compare_exchange_weak(prev, prev | bits)) {
    }
    return prev;
}
TaskHandle_t gSwitchTask = nullptr;

void romConsoleWrite(const u8* bytes, fl::size size) FL_NO_EXCEPT {
    if (fl::detail::tryConsoleWrite(bytes, size) == static_cast<fl::size>(-1)) {
        UartEsp32::writeRom(bytes, size);
    }
}

void romConsoleWriteLine(const char* str, bool newline) FL_NO_EXCEPT {
    romConsoleWrite(reinterpret_cast<const u8*>(str), fl::strlen(str)); // ok reinterpret cast - byte transport boundary.
    if (newline) {
        const u8 terminator = '\n';
        romConsoleWrite(&terminator, 1);
    }
}

void nativeConsoleWrite(const char* str, bool newline) FL_NO_EXCEPT {
    EspIO& io = EspIO::instance();
    if (newline) {
        io.println(str);
    } else {
        io.print(str);
    }
}

EspIO& nativeConsole() FL_NO_EXCEPT {
    if (!(gConsoleState.load() & kReady)) {
        if (setConsoleBits(kSwitching) & kSwitching) {
            // Another task is switching; wait for it to finish.
            while (!(gConsoleState.load() & kReady)) { vTaskDelay(1); }
            return EspIO::instance();
        }
        gSwitchTask = xTaskGetCurrentTaskHandle();
        while (gConsoleState.load() & ~kSwitching) { vTaskDelay(1); }
        // Drain ROM-written bytes before the driver install resets UART0.
        UartEsp32::waitRomIdle();
        EspIO::instance();
        gNativeConsoleWrite = &nativeConsoleWrite;
        setConsoleBits(kReady);  // published once constructed
    }
    return EspIO::instance();
}

void consoleWrite(const char* str, bool newline) FL_NO_EXCEPT {
    if (!str) { return; }
    if (!(gConsoleState.load() & kReady)) {
        // The switching task may log while constructing the console: it
        // writes ROM sequentially with its own install, so let it through.
        const fl::u32 state = gConsoleState.fetch_add(1);
        if (!(state & (kReady | kSwitching)) ||
            gSwitchTask == xTaskGetCurrentTaskHandle()) {
            romConsoleWriteLine(str, newline);
            gConsoleState.fetch_sub(1);
            return;
        }
        gConsoleState.fetch_sub(1);
        while (!(gConsoleState.load() & kReady)) { vTaskDelay(1); }
    }
    gNativeConsoleWrite(str, newline);
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
