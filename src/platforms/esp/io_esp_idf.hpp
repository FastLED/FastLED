// ESP I/O implementation - ESP-IDF ROM functions backend
// This file is included by io_esp.cpp when using ESP-IDF backend

#ifdef ESP32
#include "rom/uart.h"  // For uart_tx_one_char (ESP32 only)
#endif

#ifdef ESP8266
// ESP8266 ROM UART functions
extern "C" {
    void ets_putc(char c);
}
#endif

namespace fl {

void print_esp(const char* str) {
    if (!str)
        return;
#ifdef ESP32
    // ESP32: Use ROM uart_tx_one_char - writes to hardware FIFO (128 bytes)
    // The bootloader initializes UART0 at 115200 baud, so this works without init
    // The FIFO provides hardware buffering, so this is reasonably fast
    while (*str) {
        uart_tx_one_char(*str++);
    }
#elif defined(ESP8266)
    // ESP8266: Use ROM ets_putc function
    while (*str) {
        ets_putc(*str++);
    }
#else
    (void)str;  // Suppress unused warning
#endif
}

void println_esp(const char* str) {
    if (!str)
        return;
#ifdef ESP32
    // ESP32: Use ROM uart_tx_one_char with newline
    while (*str) {
        uart_tx_one_char(*str++);
    }
    uart_tx_one_char('\n');
#elif defined(ESP8266)
    // ESP8266: Use ROM ets_putc with newline
    while (*str) {
        ets_putc(*str++);
    }
    ets_putc('\n');
#else
    (void)str;  // Suppress unused warning
#endif
}

// Input functions
int available_esp() {
    // ESP-IDF ROM functions don't provide input availability checking
    // This would require using the UART driver API
    return 0;
}

int read_esp() {
    // ESP-IDF ROM functions don't provide convenient input reading
    // This would require using the UART driver API
    return -1;
}

} // namespace fl
