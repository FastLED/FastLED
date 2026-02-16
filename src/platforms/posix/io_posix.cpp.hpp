#pragma once

// IWYU pragma: private
#include "platforms/posix/is_posix.h"

#ifdef FL_IS_POSIX
#include <unistd.h>  // for write and fsync
#include "fl/stl/stdint.h"
#include "fl/stl/cstddef.h"

namespace fl {
namespace platforms {

// Serial initialization (no-op on POSIX)
void begin(u32 baudRate) {
    (void)baudRate;
    // POSIX host platform doesn't have serial ports - no-op
}

// Print functions
void print(const char* str) {
    if (!str) return;

    // POSIX (Linux/macOS): Use direct system calls to stderr
    // Calculate length without strlen()
    size_t len = 0;
    const char* p = str;
    while (*p++) len++;

    // Unix/Linux version
    ::write(2, str, len);  // 2 = stderr
    // Force flush stderr during testing to prevent output loss on crashes
    fsync(2);
}

void println(const char* str) {
    if (!str) return;
    print(str);
    print("\n");
}

// Input functions
int available() {
    // POSIX platforms - no input available in most cases
    // This is mainly for testing environments
    return 0;
}

int peek() {
    // POSIX platforms - no peek support
    return -1;
}

int read() {
    // POSIX platforms - no input available in most cases
    // This is mainly for testing environments
    return -1;
}

// Utility functions
bool flush(u32 timeoutMs) {
    (void)timeoutMs;
    // Force flush stderr
    fsync(2);
    return true;
}

size_t write_bytes(const u8* buffer, size_t size) {
    if (!buffer || size == 0) return 0;

    // Write raw bytes to stderr
    ssize_t written = ::write(2, buffer, size);
    return (written >= 0) ? static_cast<size_t>(written) : 0;
}

bool serial_ready() {
    // POSIX host platform always "ready" (stderr always available)
    return true;
}

bool serial_is_buffered() {
    // POSIX stderr is always "buffered" (not ROM UART - that's ESP32-specific)
    return true;
}

int readLineNative(char delimiter, char* out, int outLen) {
    (void)delimiter; (void)out; (void)outLen;
    return -1;  // Not supported on POSIX host builds
}

} // namespace platforms
} // namespace fl

#endif // FL_IS_POSIX
