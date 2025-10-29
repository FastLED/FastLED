#pragma once

// Platform-specific UART register detection is now handled in io_avr.cpp
// This keeps the header clean and avoids macro conflicts

namespace fl {

// Print functions
void print_avr(const char* str);
void println_avr(const char* str);

// Input functions
int available_avr();
int read_avr();

} // namespace fl
