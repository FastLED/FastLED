#pragma once

// FL_NOEXCEPT: expands to noexcept in C++ (reduces .eh_frame bloat on ESP32),
// noop on WASM and in C translation units.

#include "platforms/is_platform.h" // IWYU pragma: keep

#ifndef FL_NOEXCEPT
#if defined(FL_IS_WASM)
// WASM (Emscripten) does not benefit from noexcept and it can cause issues.
#define FL_NOEXCEPT
#elif defined(__cplusplus)
#define FL_NOEXCEPT noexcept
#else
#define FL_NOEXCEPT
#endif
#endif
