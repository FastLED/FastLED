#pragma once

namespace fl {

// Low-level print functions that avoid printf/sprintf dependencies
// These use the most efficient output method for each platform

// Print a string without newline
void print(const char* str);

// Print a string with newline  
void println(const char* str);

} // namespace fl
