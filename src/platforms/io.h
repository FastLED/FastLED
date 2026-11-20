// Platform I/O API - Function declarations
// Each platform provides implementations in their .cpp.hpp files

#pragma once

#include "fl/stl/stdint.h"
#include "fl/stl/noexcept.h"

namespace fl {

class string;  // fl/stl/string.h; only bound by reference here

namespace platforms {

// Longest line readLineNative() stores; longer input is consumed but dropped.
constexpr int kReadLineNativeMaxLen = 511;

// Serial initialization
void begin(u32 baudRate) FL_NO_EXCEPT;

// Print functions
void print(const char* str) FL_NO_EXCEPT;
void println(const char* str) FL_NO_EXCEPT;

// Input functions
int available() FL_NO_EXCEPT;
int peek() FL_NO_EXCEPT;
int read() FL_NO_EXCEPT;

// High-level line reading using platform's native Serial.readStringUntil()
// On Arduino, this delegates to Serial.readStringUntil() which handles
// USB CDC multi-packet transfers correctly via Stream::timedRead().
// On non-Arduino platforms, returns -1 (caller should use fl::readStringUntil fallback).
// The line is written straight into the caller's string, so no fixed-size
// line buffer lives on the caller's stack (FastLED#5307).
// @param delimiter Character to read until (not stored)
// @param out Receives the line, truncated to kReadLineNativeMaxLen characters
// @return Number of characters stored (out.size(); fewer than were read if the
//         line is longer than the cap or the string could not grow), or -1 if
//         not supported on this platform
int readLineNative(char delimiter, fl::string& out) FL_NO_EXCEPT;

// Utility functions
bool flush(u32 timeoutMs = 1000) FL_NO_EXCEPT;
size_t write_bytes(const u8* buffer, size_t size) FL_NO_EXCEPT;
bool serial_ready() FL_NO_EXCEPT;

// Test/diagnostic helper: Check if using buffered mode (not ROM UART fallback)
// Returns true if using buffered UART driver, false if using ROM UART
bool serial_is_buffered() FL_NO_EXCEPT;

} // namespace platforms
} // namespace fl
