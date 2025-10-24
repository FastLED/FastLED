#pragma once

#include <avr/io.h>
#include "lib8tion/attiny_detect.h"

// UART register access for direct output
// Some ATtiny microcontrollers don't have UART hardware at all (only USI)
#if defined(LIB8_ATTINY_NO_UART)
// These ATtiny chips have no UART hardware - only USI (Universal Serial Interface)
// No UART macros defined - UART functions will not be compiled
#elif defined(UDR0)
// Use numbered register names for multi-UART AVRs (like ATmega328P, ATmega2560, etc.)
#define UART_UDR UDR0
#define UART_UCSRA UCSR0A
#define UART_UDRE UDRE0
#define UART_RXC RXC0
#elif defined(UDR)
// Use non-numbered register names for ATtiny with UART and single-UART AVRs
#define UART_UDR UDR
#define UART_UCSRA UCSRA
#define UART_UDRE UDRE
#define UART_RXC RXC
#endif

namespace fl {

// Print functions
void print_avr(const char* str);
void println_avr(const char* str);

// Input functions
int available_avr();
int read_avr();

} // namespace fl
