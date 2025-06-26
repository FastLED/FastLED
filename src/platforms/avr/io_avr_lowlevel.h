#pragma once

#include <avr/io.h>

// UART register access for direct output
// Some ATtiny microcontrollers don't have UART hardware at all (only USI)
#if defined(__AVR_ATtiny25__) || defined(__AVR_ATtiny45__) || defined(__AVR_ATtiny85__) || \
    defined(__AVR_ATtiny13__) || defined(__AVR_ATtiny13A__)
// These ATtiny chips have no UART hardware - only USI (Universal Serial Interface)
// No UART macros defined - UART functions will not be compiled
#elif defined(__AVR_ATtiny4313__) || defined(__AVR_ATtiny2313__) || defined(__AVR_ATtiny2313A__) || \
      defined(__AVR_ATtiny24__) || defined(__AVR_ATtiny44__) || defined(__AVR_ATtiny84__) || \
      defined(__AVR_ATtiny167__) || defined(__AVR_ATtiny87__) || defined(__AVR_ATtiny48__) || \
      defined(__AVR_ATtiny88__) || defined(__AVR_ATtiny841__) || defined(__AVR_ATtiny441__) || \
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

// Low-level AVR UART helper functions
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

static bool avr_uart_is_initialized() {
    // Basic check if UART seems to be initialized
    // 0xFF usually indicates uninitialized registers
    return (UART_UCSRA != 0xFF);
}
#endif

} // namespace fl
