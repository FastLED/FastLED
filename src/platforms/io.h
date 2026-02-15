// Platform I/O API - Function declarations
// Each platform provides implementations in their .cpp.hpp files

#pragma once

#include "fl/stl/stdint.h"

namespace fl {
namespace platforms {

// Serial initialization
void begin(u32 baudRate);

// Print functions
void print(const char* str);
void println(const char* str);

// Input functions
int available();
int peek();
int read();

// High-level line reading using platform's native Serial.readStringUntil()
// On Arduino, this delegates to Serial.readStringUntil() which handles
// USB CDC multi-packet transfers correctly via Stream::timedRead().
// On non-Arduino platforms, returns false (caller should use fl::readStringUntil fallback).
// @param delimiter Character to read until
// @param out Buffer to write the line into (allocated by caller)
// @param outLen Max buffer size
// @return Number of characters read, or -1 if not supported on this platform
int readLineNative(char delimiter, char* out, int outLen);

// Utility functions
bool flush(u32 timeoutMs = 1000);
size_t write_bytes(const u8* buffer, size_t size);
bool serial_ready();

// Test/diagnostic helper: Check if using buffered mode (not ROM UART fallback)
// Returns true if using buffered UART driver, false if using ROM UART
bool serial_is_buffered();

} // namespace platforms
} // namespace fl
