#pragma once

#include <avr/io.h>

// UART register access for direct output
#ifdef UDR0
#define UART_UDR UDR0
#define UART_UCSRA UCSR0A
#define UART_UDRE UDRE0
#elif defined(UDR)
#define UART_UDR UDR
#define UART_UCSRA UCSRA
#define UART_UDRE UDRE
#endif

namespace fl {

// Helper function for AVR UART output
#ifdef UART_UDR
static void avr_uart_putchar(char c) {
    // Wait for empty transmit buffer
    while (!(UART_UCSRA & (1 << UART_UDRE)));
    // Put data into buffer, sends the data
    UART_UDR = c;
}
#endif

inline void print_avr(const char* str) {
    if (!str) return;
    
#ifdef UART_UDR
    // AVR: Use native UART registers first (if UART is initialized)
    const char* p = str;
    
    // Check if UART seems to be initialized (basic check)
    if (UART_UCSRA != 0xFF) {  // 0xFF usually indicates uninitialized
        while (*p) {
            avr_uart_putchar(*p++);
        }
    } else {
        // Fallback to Arduino Serial if available
        #ifdef ARDUINO_H
        if (Serial) {
            Serial.print(str);
        }
        #endif
    }
#else
    // Fallback to Arduino Serial if UART registers not available
    #ifdef ARDUINO_H
    if (Serial) {
        Serial.print(str);
    }
    #endif
#endif
}

inline void println_avr(const char* str) {
    if (!str) return;
    print_avr(str);
    print_avr("\n");
}

} // namespace fl
