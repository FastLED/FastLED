// ok no namespace fl
#pragma once

// Desktop/generic platform integer type definitions
// Dispatches to platform-specific files for macOS, Linux, Windows, etc.
//
// IMPORTANT: macOS and Linux LP64 systems define uint64_t differently:
// - macOS: uint64_t is 'unsigned long long' (even in 64-bit mode)
// - Linux: uint64_t is 'unsigned long' (in 64-bit LP64 mode)
//
// See individual platform files for details.

#if defined(__APPLE__)
    // macOS (all versions)
    #include "platforms/shared/int_macos.h"
#elif defined(_WIN32) || defined(_WIN64) || defined(_MSC_VER)
    // Windows (all versions)
    #include "platforms/shared/int_windows.h"
#elif defined(__linux__) && (defined(__LP64__) || defined(_LP64))
    // Linux LP64 (64-bit Linux)
    #include "platforms/shared/int_linux.h"
#else
    // Generic/32-bit/unknown platforms
    #include "platforms/shared/int_generic.h"
#endif
