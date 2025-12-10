// ESP I/O implementation - ESP32 ROM functions

#ifdef ESP32

#include "io_esp.h"

#include "rom/uart.h"  // For uart_tx_one_char

namespace fl {

void print_esp(const char *str) {
    if (!str)
        return;
    // Use ROM uart_tx_one_char - writes to hardware FIFO (128 bytes)
    // The FIFO provides hardware buffering, so this is reasonably fast
    while (*str) {
        uart_tx_one_char(*str++);
    }
}

void println_esp(const char *str) {
    if (!str)
        return;
    // Use ROM uart_tx_one_char with newline
    while (*str) {
        uart_tx_one_char(*str++);
    }
    uart_tx_one_char('\n');
}

int available_esp() {
    // Not implemented yet
    return 0;
}

int read_esp() {
    // Not implemented yet
    return -1;
}

} // namespace fl

#endif // ESP32
