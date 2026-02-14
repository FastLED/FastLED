
#pragma once

// IWYU pragma: private


// Use ESP-IDF UART + USB-Serial JTAG driver via fl::platforms wrapper
// Note: Implementation files are included via drivers/_build.hpp
#include "platforms/esp/32/detail/io_esp_jtag_or_uart.hpp"

namespace fl {
namespace platforms {

// External API functions - simple delegation to EspIO singleton
void begin(u32 baudRate) {
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

bool flush(u32 timeoutMs = 1000) {
    return EspIO::instance().flush(timeoutMs);
}

size_t write_bytes(const u8* buffer, size_t size) {
    return EspIO::instance().writeBytes(buffer, size);
}

bool serial_ready() {
    return EspIO::instance().isReady();
}

// Test/diagnostic helper: Check if using buffered mode (not ROM UART fallback)
bool serial_is_buffered() {
    return EspIO::instance().isBufferedMode();
}

int readLineNative(char delimiter, char* out, int outLen) {
    (void)delimiter; (void)out; (void)outLen;
    return -1;  // Not supported on ESP-IDF (non-Arduino) builds
}

}  // namespace platforms
}  // namespace fl
