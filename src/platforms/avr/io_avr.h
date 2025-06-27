#pragma once

#include <avr/io.h>

// UART register access for direct output
// Some ATtiny microcontrollers don't have UART hardware at all (only USI)
#if defined(__AVR_ATtiny25__) || defined(__AVR_ATtiny45__) || defined(__AVR_ATtiny85__) || \
    defined(__AVR_ATtiny13__) || defined(__AVR_ATtiny13A__) || defined(__AVR_ATtiny88__)
// These ATtiny chips have no UART hardware - only USI (Universal Serial Interface)
// No UART macros defined - UART functions will not be compiled
#elif defined(__AVR_ATtiny4313__) || defined(__AVR_ATtiny2313__) || defined(__AVR_ATtiny2313A__) || \
      defined(__AVR_ATtiny24__) || defined(__AVR_ATtiny44__) || defined(__AVR_ATtiny84__) || \
      defined(__AVR_ATtiny167__) || defined(__AVR_ATtiny87__) || defined(__AVR_ATtiny48__) || \
      defined(__AVR_ATtiny841__) || defined(__AVR_ATtiny441__) || \
      (defined(UDR) && !defined(UDR0))
// Use non-numbered register names for ATtiny and single-UART AVRs
#define UART_UDR UDR
#define UART_UCSRA UCSRA
#define UART_UDRE UDRE
#define UART_RXC RXC
#elif defined(UDR0)
// Use numbered register names for multi-UART AVRs (like ATmega328P, ATmega2560, etc.)
#define UART_UDR UDR0
#define UART_UCSRA UCSR0A
#define UART_UDRE UDRE0
#define UART_RXC RXC0
#elif defined(UDR)
// Fallback for other single-UART AVRs
#define UART_UDR UDR
#define UART_UCSRA UCSRA
#define UART_UDRE UDRE
#define UART_RXC RXC
#endif

namespace fl {

// Helper functions for AVR UART I/O
#ifdef UART_UDR
static void avr_uart_putchar(char c) {
    // Wait for empty transmit buffer
    while (!(UART_UCSRA & (1 << UART_UDRE)));
    // Put data into buffer, sends the data
    UART_UDR = c;
}

static int avr_uart_available() {
    // Check if data is available in receive buffer
    return (UART_UCSRA & (1 << UART_RXC)) ? 1 : 0;
}

static int avr_uart_read() {
    // Check if data is available
    if (UART_UCSRA & (1 << UART_RXC)) {
        return UART_UDR;
    }
    return -1;
}
#endif

// Print functions
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

// Input functions
inline int available_avr() {
#ifdef UART_UDR
    // AVR: Use native UART registers first (if UART is initialized)
    // Check if UART seems to be initialized (basic check)
    if (UART_UCSRA != 0xFF) {  // 0xFF usually indicates uninitialized
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

inline int read_avr() {
#ifdef UART_UDR
    // AVR: Use native UART registers first (if UART is initialized)
    // Check if UART seems to be initialized (basic check)
    if (UART_UCSRA != 0xFF) {  // 0xFF usually indicates uninitialized
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
