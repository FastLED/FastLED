#pragma once

// IWYU pragma: private

#include "platforms/win/is_win.h"

#ifdef FL_IS_WIN

// IWYU pragma: begin_keep
#include <io.h>  // for _write
// IWYU pragma: end_keep
#include "fl/stl/stdint.h"
#include "fl/stl/cstddef.h"
#include "fl/stl/noexcept.h"

namespace fl {
namespace platforms {

// Serial initialization (no-op on Windows)
void begin(u32 baudRate) FL_NOEXCEPT {
    (void)baudRate;
    // Windows host platform doesn't have serial ports - no-op
}

// Print functions
void print(const char* str) FL_NOEXCEPT {
    if (!str) return;

    // Windows: Use direct system calls to stderr
    // Calculate length without strlen()
    size_t len = 0;
    const char* p = str;
    while (*p++) len++;

    // Windows version: _write to stderr (2 is stderr file descriptor)
    // Note: _write performs native I/O and doesn't require additional flushing
    _write(2, str, static_cast<unsigned int>(len));  // 2 = stderr
}

void println(const char* str) FL_NOEXCEPT {
    if (!str) return;
    print(str);
    print("\n");
}

// Input functions
int available() FL_NOEXCEPT {
    // Windows testing - no input available in most cases
    // This is mainly for testing environments
    return 0;
}

int peek() FL_NOEXCEPT {
    // Windows testing - no peek support
    return -1;
}

int read() FL_NOEXCEPT {
    // Windows testing - no input available in most cases
    // This is mainly for testing environments
    return -1;
}

// Utility functions
//
// Honors `timeoutMs` per the FastLED/FastLED#2668 contract: `flush(0)` must
// return immediately. `_write(2, ...)` to stderr is unbuffered on Windows,
// so any positive `timeoutMs` is also satisfied trivially (no draining
// work to perform). The explicit `timeoutMs == 0` branch documents the
// contract and prevents future drift if a buffered sink is ever added.
bool flush(u32 timeoutMs) FL_NOEXCEPT {
    if (timeoutMs == 0) {
        return true;
    }
    // _write to stderr is unbuffered on Windows - nothing to drain.
    return true;
}

size_t write_bytes(const u8* buffer, size_t size) FL_NOEXCEPT {
    if (!buffer || size == 0) return 0;

    // Write raw bytes to stderr
    int written = _write(2, buffer, static_cast<unsigned int>(size));
    return (written >= 0) ? static_cast<size_t>(written) : 0;
}

bool serial_ready() FL_NOEXCEPT {
    // Windows host platform always "ready" (stderr always available)
    return true;
}

bool serial_is_buffered() FL_NOEXCEPT {
    // Windows stderr is always "buffered" (not ROM UART - that's ESP32-specific)
    return true;
}

int readLineNative(char delimiter, char* out, int outLen) FL_NOEXCEPT {
    (void)delimiter; (void)out; (void)outLen;
    return -1;  // Not supported on Windows host builds
}

} // namespace platforms
} // namespace fl

#endif // FL_IS_WIN
