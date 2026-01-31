#pragma once

///////////////////////////////////////////////////////////////////////////////
// FastLED C Standard Library Compatibility Header
//
// This header provides a compatibility layer for third-party code that
// expects standard C library functions like malloc, free, exit, etc.
///////////////////////////////////////////////////////////////////////////////

#include "fl/stl/stdint.h"

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

// C-style comparison function type for qsort
typedef int (*qsort_compare_fn)(const void*, const void*);

// qsort - Quick sort function compatible with C stdlib qsort
// Sorts an array of elements using the provided comparison function
void qsort(void* base, size_t nmemb, size_t size, qsort_compare_fn compar);

} // namespace fl
