#pragma once

#include <stddef.h>
#include <string.h>

// We meed to define a missing _write function for the Teensy LC.
// This regardless of whether the user prints or not, because of the SD card
// system, the platformio build system will try to link to libc, which needs
// to resolve _write.
// https://forum.pjrc.com/index.php?threads/undefined-reference-to-_write.71420/
#if defined(ARDUINO)

#include <Arduino.h> // ok include
#include <unistd.h>
#include <cerrno>
#undef errno
extern int errno;
// ^^^^ Add these 4 lines to your includes


extern "C" {
int _write(int file, const void *buf, size_t len) {
  if (len == 0) {
    return 0;
  }
  static Print *volatile stdPrint = &Serial;

  Print *out;

  // Send both stdout and stderr to stdPrint
  if (file == STDOUT_FILENO || file == STDERR_FILENO) {
    out = stdPrint;
  } else if (file == STDIN_FILENO) {
    errno = EBADF;
    return -1;
  } else {
    out = (Print *)file;
  }

  if (out == nullptr) {
    return len;
  }
  return out->write((const uint8_t *)buf, len);
}
}  // extern "C"

inline int _platform_write(const char* str) {
    return _write(STDOUT_FILENO, str, strlen(str));
}

#else

inline int _platform_write(const char* str) {
    (void)str;
    return 0;
}

#endif  // ARDUINO

namespace fl {

// Low-level Teensy LC print functions that avoid _write dependencies
// These use no-op implementations to prevent linker errors

inline void print_teensy_lc(const char* str) {
    if (!str) return;
    _platform_write(str);
}

inline void println_teensy_lc(const char* str) {
    if (!str) return;
    _platform_write(str);
    _platform_write("\n");
}

// Input functions - no-op implementations for Teensy LC
inline int available_teensy_lc() {
    // No-op implementation to avoid dependencies
    // Teensy LC users should use Serial.available() directly for input
    return 0;
}

inline int read_teensy_lc() {
    // No-op implementation to avoid dependencies
    // Teensy LC users should use Serial.read() directly for input
    return -1;
}

} // namespace fl
