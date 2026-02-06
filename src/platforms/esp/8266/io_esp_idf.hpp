// ESP8266 I/O implementation - ROM UART functions backend
// This file is included by io_esp_idf.hpp for ESP8266 platforms
// Uses ROM ets_putc function for character-by-character output

// ESP8266 ROM UART functions
extern "C" {
    void ets_putc(char c);
}

namespace fl {
namespace platforms {

void print(const char* str) {
    if (!str)
        return;

    // ESP8266: Use ROM ets_putc function
    while (*str) {
        ets_putc(*str++);
    }
}

void println(const char* str) {
    if (!str)
        return;

    // ESP8266: Use ROM ets_putc with newline
    while (*str) {
        ets_putc(*str++);
    }
    ets_putc('\n');
}

// Input functions
int available() {
    // ESP8266 ROM functions don't provide input availability checking
    return 0;
}

int read() {
    // ESP8266 ROM functions don't provide convenient input reading
    return -1;
}

} // namespace platforms
} // namespace fl
