#pragma once
#include "platforms/wasm/is_wasm.h"

#ifdef FL_IS_WASM
/// WASM I/O implementation
/// Consolidates print and input functions for the WASM platform.
/// Print functions use printf to output to JavaScript console.
/// Input functions always return empty/no-data (WASM doesn't support serial input by default).

#include <emscripten.h>
#include <cstdio>
#include "fl/stl/cstddef.h"
#include "fl/stl/stdint.h"

namespace fl {
namespace platforms {

/// Initialize serial (no-op on WASM)
void begin(u32 baudRate) {
    (void)baudRate;
    // WASM doesn't have serial ports - no-op
}

/// Print string to console (via printf)
void print(const char* str) {
    if (!str) return;
    ::printf("%s", str);
}

/// Print string with newline to console (via printf)
void println(const char* str) {
    if (!str) return;
    ::printf("%s\n", str);
}

/// Always returns 0 (no input available in WASM)
int available() {
    return 0;
}

/// Always returns -1 (no peek support in WASM)
int peek() {
    return -1;
}

/// Always returns -1 (no data to read in WASM)
int read() {
    return -1;
}

/// Flush output (no-op on WASM - printf is unbuffered)
bool flush(u32 timeoutMs) {
    (void)timeoutMs;
    return true;
}

/// Write raw bytes to console
size_t write_bytes(const u8* buffer, size_t size) {
    if (!buffer || size == 0) return 0;

    // Write bytes as binary data to console (hex output)
    for (size_t i = 0; i < size; i++) {
        ::printf("%02X ", buffer[i]);
    }
    return size;
}

/// Check if serial is ready (always true on WASM)
bool serial_ready() {
    return true;
}

} // namespace platforms
} // namespace fl

#endif // FL_IS_WASM
