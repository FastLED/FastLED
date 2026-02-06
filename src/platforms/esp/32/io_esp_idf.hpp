
#pragma once


// Use ESP-IDF UART driver via fl::platforms wrapper
// Note: uart_esp32_idf.hpp is included via drivers/_build.hpp
#include "platforms/esp/32/detail/io_uart.hpp"

namespace fl {
namespace platforms {

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

}  // namespace platforms
}  // namespace fl
