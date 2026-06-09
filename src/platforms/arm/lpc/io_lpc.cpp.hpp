#pragma once

// IWYU pragma: private

#include "platforms/arm/lpc/is_lpc.h"

#ifdef FL_IS_ARM_LPC

#include "fl/stl/cstddef.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/stdint.h"
#include "platforms/arm/is_arm.h"
#include "platforms/io.h"

namespace fl {
namespace platforms {

void begin(u32 baudRate) FL_NOEXCEPT {
    (void)baudRate;
}

void print(const char* str) FL_NOEXCEPT {
    (void)str;
}

void println(const char* str) FL_NOEXCEPT {
    (void)str;
}

int available() FL_NOEXCEPT {
    return 0;
}

int peek() FL_NOEXCEPT {
    return -1;
}

int read() FL_NOEXCEPT {
    return -1;
}

int readLineNative(char delimiter, char* out, int outLen) FL_NOEXCEPT {
    (void)delimiter;
    (void)out;
    (void)outLen;
    return -1;
}

bool flush(u32 timeoutMs) FL_NOEXCEPT {
    (void)timeoutMs;
    return true;
}

size_t write_bytes(const u8* buffer, size_t size) FL_NOEXCEPT {
    (void)buffer;
    (void)size;
    return 0;
}

bool serial_ready() FL_NOEXCEPT {
    return false;
}

bool serial_is_buffered() FL_NOEXCEPT {
    return false;
}

}  // namespace platforms
}  // namespace fl

#endif  // FL_IS_ARM_LPC
