// Crash handler initialization for runner.exe
// This file is compiled into a static library and linked with runner.exe
// to provide crash handling before any test DLLs are loaded.

// Workaround for Clang 19 AVX-512 intrinsics bug on Windows
#ifdef _WIN32
#define __AVX512BITALG__
#define __AVX512VPOPCNTDQ__
#endif

// Suppress -Wpragma-pack warnings from Windows SDK headers
#ifdef _WIN32
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpragma-pack"
#endif
#endif

#include "../crash_handler.h"

#ifdef _WIN32
#ifdef __clang__
#pragma clang diagnostic pop
#endif
#endif

// Setup crash handler - this is called from runner.exe main()
// before loading any test DLLs
extern "C" void runner_setup_crash_handler() {
    setup_crash_handler();
}
