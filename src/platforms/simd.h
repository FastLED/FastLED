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


// Platform-specific SIMD enabled for performance optimization
#include "platforms/is_platform.h"
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
    // x86/x64 platforms (SSE2/AVX intrinsics)
    #include "platforms/shared/simd_x86.hpp"
#else
    // No SIMD support - use scalar fallback
    // Covers: AVR, ESP8266, ARM without NEON, WASM, and other platforms
    #include "platforms/shared/simd_noop.hpp"
#endif
