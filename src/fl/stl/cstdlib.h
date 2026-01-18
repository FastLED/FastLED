#pragma once

///////////////////////////////////////////////////////////////////////////////
// FastLED C Standard Library Compatibility Header
//
// This header provides a compatibility layer for third-party code that
// expects standard C library functions like malloc, free, exit, etc.
///////////////////////////////////////////////////////////////////////////////

namespace fl {

// Convert string to long integer
// Similar to standard strtol but without locale support
long strtol(const char* str, char** endptr, int base);

// Convert string to unsigned long integer
unsigned long strtoul(const char* str, char** endptr, int base);

// Convert string to integer
int atoi(const char* str);

// Convert string to long
long atol(const char* str);

// Convert string to double
double strtod(const char* str, char** endptr);

} // namespace fl
