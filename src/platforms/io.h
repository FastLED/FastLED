// Platform I/O API - Function declarations
// Each platform provides implementations in their .cpp.hpp files

#pragma once

#include "fl/stl/stdint.h"

namespace fl {
namespace platforms {

// Serial initialization
void begin(uint32_t baudRate);

// Print functions
void print(const char* str);
void println(const char* str);

// Input functions
int available();
int peek();
int read();

// Utility functions
bool flush(uint32_t timeoutMs = 1000);
size_t write_bytes(const uint8_t* buffer, size_t size);
bool serial_ready();

} // namespace platforms
} // namespace fl
