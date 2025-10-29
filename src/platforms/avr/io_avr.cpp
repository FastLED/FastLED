#ifdef __AVR__

#include "io_avr.h"

#ifdef ARDUINO_H
#include "Arduino.h"
#endif

namespace fl {

// Platform-specific UART register detection and helper functions
// ATtiny chips with UART use non-numbered registers (UCSRA, UDR, etc.)
// ATmega chips with multiple UARTs use numbered registers (UCSR0A, UDR0, etc.)

#if defined(UCSRA) && defined(UDR)
// Single UART devices (ATtiny2313, ATtiny4313, older ATmega, etc.)
#define FL_UART_UCSRA UCSRA
#define FL_UART_UDR UDR
#define FL_UART_UDRE UDRE
#define FL_UART_RXC RXC
#define FL_HAS_UART 1
#elif defined(UCSR0A) && defined(UDR0)
// Multi-UART devices (ATmega328P, ATmega2560, etc.)
#define FL_UART_UCSRA UCSR0A
#define FL_UART_UDR UDR0
#define FL_UART_UDRE UDRE0
#define FL_UART_RXC RXC0
#define FL_HAS_UART 1
#endif

// Helper functions for AVR UART I/O
#ifdef FL_HAS_UART
static void avr_uart_putchar(char c) {
    // Wait for empty transmit buffer
    while (!(FL_UART_UCSRA & (1 << FL_UART_UDRE)));
    // Put data into buffer, sends the data
    FL_UART_UDR = c;
}

static int avr_uart_available() {
    // Check if data is available in receive buffer
    return (FL_UART_UCSRA & (1 << FL_UART_RXC)) ? 1 : 0;
}

static int avr_uart_read() {
    // Check if data is available
    if (FL_UART_UCSRA & (1 << FL_UART_RXC)) {
        return FL_UART_UDR;
    }
    return -1;
}
#endif

// Print functions
void print_avr(const char* str) {
    if (!str) return;

#ifdef FL_HAS_UART
    // AVR: Use native UART registers first (if UART is initialized)
    const char* p = str;

    // Check if UART seems to be initialized (basic check)
    if (FL_UART_UCSRA != 0xFF) {  // 0xFF usually indicates uninitialized
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

void println_avr(const char* str) {
    if (!str) return;
    print_avr(str);
    print_avr("\n");
}

// Input functions
int available_avr() {
#ifdef FL_HAS_UART
    // AVR: Use native UART registers first (if UART is initialized)
    // Check if UART seems to be initialized (basic check)
    if (FL_UART_UCSRA != 0xFF) {  // 0xFF usually indicates uninitialized
        return avr_uart_available();
    } else {
        // Fallback to Arduino Serial if available
        #ifdef ARDUINO_H
        if (Serial) {
            return Serial.available();
        }
        #endif
    }
#else
    // Fallback to Arduino Serial if UART registers not available
    #ifdef ARDUINO_H
    if (Serial) {
        return Serial.available();
    }
    #endif
#endif
    return 0;
}

int read_avr() {
#ifdef FL_HAS_UART
    // AVR: Use native UART registers first (if UART is initialized)
    // Check if UART seems to be initialized (basic check)
    if (FL_UART_UCSRA != 0xFF) {  // 0xFF usually indicates uninitialized
        return avr_uart_read();
    } else {
        // Fallback to Arduino Serial if available
        #ifdef ARDUINO_H
        if (Serial && Serial.available()) {
            return Serial.read();
        }
        #endif
    }
#else
    // Fallback to Arduino Serial if UART registers not available
    #ifdef ARDUINO_H
    if (Serial && Serial.available()) {
        return Serial.read();
    }
    #endif
#endif
    return -1;
}

} // namespace fl

#endif // __AVR__
