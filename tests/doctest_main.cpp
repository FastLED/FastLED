// Workaround for Clang 19 AVX-512 intrinsics bug on Windows
// The clang-tool-chain version 19 has buggy AVX-512 intrinsics that use __builtin_elementwise_popcount
// which is not available in this build. We prevent those headers from being included.
#ifdef _WIN32
// Prevent immintrin.h from including the buggy AVX-512 headers
#define __AVX512BITALG__
#define __AVX512VPOPCNTDQ__
#endif

// Fix INPUT macro conflict between Arduino and Windows headers
#ifdef INPUT
#define ARDUINO_INPUT_BACKUP INPUT
#undef INPUT
#endif

// Suppress -Wpragma-pack warnings from Windows SDK headers
// These warnings come from Microsoft's own headers (winnt.h, wingdi.h, etc.) which use
// #pragma pack(push/pop) to control struct alignment. Clang warns about these alignment
// changes, but they're intentional and properly balanced in the SDK headers.
#ifdef _WIN32
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpragma-pack"
#endif
#endif

#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest.h"

#ifdef _WIN32
#ifdef __clang__
#pragma clang diagnostic pop
#endif
#endif

// Restore Arduino INPUT macro after Windows headers
#ifdef ARDUINO_INPUT_BACKUP
#define INPUT ARDUINO_INPUT_BACKUP
#undef ARDUINO_INPUT_BACKUP
#endif

#ifdef ENABLE_CRASH_HANDLER
#include "crash_handler.h"
#endif

// This file contains the main function for doctest
// It will be compiled once and linked to all test executables

#ifdef _WIN32
// Windows-specific entry point
#include <windows.h>

// Define the main entry point for Windows
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, INT nCmdShow) {
    (void)hInstance; (void)hPrevInstance; (void)lpCmdLine; (void)nCmdShow; // Suppress unused parameter warnings
#ifdef ENABLE_CRASH_HANDLER
    setup_crash_handler();
#endif
    return doctest::Context().run();
}

// Also provide a standard main for compatibility
int main(int argc, char** argv) {
#ifdef ENABLE_CRASH_HANDLER
    setup_crash_handler();
#endif
    return doctest::Context(argc, argv).run();
}
#else
// Standard entry point for non-Windows platforms
int main(int argc, char** argv) {
#ifdef ENABLE_CRASH_HANDLER
    setup_crash_handler();
#endif
    return doctest::Context(argc, argv).run();
}
#endif
