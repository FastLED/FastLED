#pragma once

namespace fl {

// Low-level print functions that avoid printf/sprintf dependencies
// These use the most efficient output method for each platform

// Print a string without newline
void print(const char* str);

// Print a string with newline  
void println(const char* str);

// Low-level input functions that provide Serial-style read functionality
// These use the most efficient input method for each platform

// Returns the number of bytes available to read from input stream
// Returns 0 if no data is available
int available();

// Reads the next byte from input stream
// Returns the byte value (0-255) if data is available
// Returns -1 if no data is available
int read();

} // namespace fl
