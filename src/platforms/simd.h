// ok no namespace fl
#pragma once

/// @file simd.h
/// Platform-dispatch trampoline for SIMD operations
///
/// Routes SIMD operations to platform-specific implementations following
/// the coarse-to-fine delegation pattern used throughout FastLED.
///
/// Platform implementations provide functions in fl::simd::platform namespace.
/// The public API in fl::simd delegates to the appropriate platform implementation.

// Include platform-specific SIMD implementation headers
// Each platform defines types and functions in fl::simd::platform namespace


// All simd operations are disabled for now. If you want to test these simd
// operations, then please test them and send fastled issues a pull request.
#include "platforms/shared/simd_stub.hpp"

// #include "platforms/is_platform.h"
// #if defined(ESP32)
//     // ESP32 family dispatcher (routes to Xtensa or RISC-V implementations)
//     #include "platforms/esp/32/simd_esp32.hpp"
// #elif defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
//     // x86/x64 platforms (SSE2/AVX intrinsics)
//     #include "platforms/shared/simd_x86.hpp"
// #elif defined(__ARM_NEON) || defined(__ARM_NEON__)
//     // ARM platforms with NEON support
//     #include "platforms/arm/simd_arm_neon.hpp"
// #else
//     // No SIMD support - use scalar fallback
//     // Covers: AVR, ESP8266, ARM without NEON, WASM, and other platforms
//     #include "platforms/shared/simd_stub.hpp"
// #endif
