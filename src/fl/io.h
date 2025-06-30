#pragma once

#define FL_IO_H_INCLUDED

#ifdef FASTLED_TESTING
#include "fl/function.h"
#endif

namespace fl {

// Low-level print functions that avoid printf/sprintf dependencies
// These use the most efficient output method for each platform

// Print a string without newline
void print(const char* str);

// Print a string with newline  
#ifndef FL_DBG_PRINTLN_DECLARED
void println(const char* str);
#else
// Declaration already exists from fl/dbg.h
#endif

// Low-level input functions that provide Serial-style read functionality
// These use the most efficient input method for each platform

// Returns the number of bytes available to read from input stream
// Returns 0 if no data is available
int available();

// Reads the next byte from input stream
// Returns the byte value (0-255) if data is available
// Returns -1 if no data is available
int read();

#ifdef FASTLED_TESTING

// Testing function handler types
using print_handler_t = fl::function<void(const char*)>;
using println_handler_t = fl::function<void(const char*)>;
using available_handler_t = fl::function<int()>;
using read_handler_t = fl::function<int()>;

// Inject function handlers for testing
void inject_print_handler(const print_handler_t& handler);
void inject_println_handler(const println_handler_t& handler);
void inject_available_handler(const available_handler_t& handler);
void inject_read_handler(const read_handler_t& handler);

// Clear all injected handlers (restores default behavior)
void clear_io_handlers();

// Clear individual handlers
void clear_print_handler();
void clear_println_handler();
void clear_available_handler();
void clear_read_handler();

#endif // FASTLED_TESTING

} // namespace fl
