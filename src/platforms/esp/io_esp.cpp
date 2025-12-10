// ESP I/O implementation - ESP-IDF native UART driver

#ifdef __ESP_IDF__

#include "io_esp.h"

#include "driver/uart.h"

namespace fl {

// Inline strlen to avoid including string.h
static size_t _esp_strlen(const char *str) {
    size_t len = 0;
    while (str && *str++)
        len++;
    return len;
}

void print_esp(const char *str) {
    if (!str)
        return;
    // Use direct UART write instead of ESP_LOGI to avoid vfprintf dependency
    uart_write_bytes(UART_NUM_0, str, _esp_strlen(str));
}

void println_esp(const char *str) {
    if (!str)
        return;
    // Use direct UART write with newline
    uart_write_bytes(UART_NUM_0, str, _esp_strlen(str));
    uart_write_bytes(UART_NUM_0, "\n", 1);
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

#endif // __ESP_IDF__
