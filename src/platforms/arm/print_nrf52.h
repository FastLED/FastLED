#pragma once

#include "fl/stdint.h"

// nRF52 UART register definitions
#define NRF_UART0_BASE        0x40002000UL
#define UART_ENABLE_OFFSET    0x500
#define UART_PSELTXD_OFFSET   0x50C
#define UART_STARTTX_OFFSET   0x008
#define UART_TXD_OFFSET       0x51C
#define UART_EVENTS_TXDRDY_OFFSET 0x11C

namespace fl {

// Helper function for nRF52 UART output
static inline void nrf_uart_putchar(char c) {
    volatile uint32_t* uart_base = (volatile uint32_t*)NRF_UART0_BASE;
    // Check if UART is enabled
    if (*(uart_base + (UART_ENABLE_OFFSET / 4)) != 0) {
        *(uart_base + (UART_TXD_OFFSET / 4)) = c;
        *(uart_base + (UART_STARTTX_OFFSET / 4)) = 1;
        // Wait for transmission to complete
        while (*(uart_base + (UART_EVENTS_TXDRDY_OFFSET / 4)) == 0);
        *(uart_base + (UART_EVENTS_TXDRDY_OFFSET / 4)) = 0;
    }
}

inline void print_nrf52(const char* str) {
    if (!str) return;
    
    // nRF52: Use native UART first
    const char* p = str;
    volatile uint32_t* uart_base = (volatile uint32_t*)NRF_UART0_BASE;
    
    if (*(uart_base + (UART_ENABLE_OFFSET / 4)) != 0) {
        // UART is enabled, use native output
        while (*p) {
            nrf_uart_putchar(*p++);
        }
    } else {
        // Fallback to Arduino Serial if available
        #ifdef ARDUINO_H
        if (Serial) {
            Serial.print(str);
        }
        #endif
    }
}

inline void println_nrf52(const char* str) {
    if (!str) return;
    print_nrf52(str);
    print_nrf52("\n");
}

} // namespace fl
