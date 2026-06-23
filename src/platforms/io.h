// Platform I/O API - Function declarations
// Each platform provides implementations in their .cpp.hpp files

#pragma once

#include "fl/stl/stdint.h"
#include "fl/stl/noexcept.h"

namespace fl {
namespace platforms {

// Serial initialization
void begin(u32 baudRate) FL_NOEXCEPT;

// Print functions
void print(const char* str) FL_NOEXCEPT;
void println(const char* str) FL_NOEXCEPT;

// Input functions
int available() FL_NOEXCEPT;
int peek() FL_NOEXCEPT;
int read() FL_NOEXCEPT;

// High-level line reading using platform's native Serial.readStringUntil()
// On Arduino, this delegates to Serial.readStringUntil() which handles
// USB CDC multi-packet transfers correctly via Stream::timedRead().
// On non-Arduino platforms, returns false (caller should use fl::readStringUntil fallback).
// @param delimiter Character to read until
// @param out Buffer to write the line into (allocated by caller)
// @param outLen Max buffer size
// @return Number of characters read, or -1 if not supported on this platform
int readLineNative(char delimiter, char* out, int outLen) FL_NOEXCEPT;

// Utility functions
bool flush(u32 timeoutMs = 1000) FL_NOEXCEPT;
size_t write_bytes(const u8* buffer, size_t size) FL_NOEXCEPT;
bool serial_ready() FL_NOEXCEPT;

// Test/diagnostic helper: Check if using buffered mode (not ROM UART fallback)
// Returns true if using buffered UART driver, false if using ROM UART
bool serial_is_buffered() FL_NOEXCEPT;

} // namespace platforms
} // namespace fl
