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
#include "platforms/is_platform.h"  // IWYU pragma: keep
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
    // x86/x64 platforms (SSE2/AVX intrinsics)
    #include "platforms/shared/simd_x86.hpp"  // IWYU pragma: keep
#elif defined(FL_IS_ESP32)
    // ESP32 platforms (Xtensa PIE or RISC-V scalar)
    #include "platforms/esp/32/simd_esp32.hpp"  // IWYU pragma: keep
#elif defined(__ARM_FEATURE_DSP) && (__ARM_FEATURE_DSP + 0) == 1
    // ARMv7E-M DSP extension: Cortex-M4 / M4F / M7 (Teensy 3.x and 4.x,
    // Apollo3, SAMD51, nRF52, STM32F4/F7, ...). Packed-byte and packed-half
    // arithmetic via UQADD8 / UQSUB8 / UADD16 / etc. See issue #2628.
    #include "platforms/arm/teensy/simd_arm_dsp.hpp"  // IWYU pragma: keep
#else
    // No SIMD support - use scalar fallback
    // Covers: AVR, ESP8266, ARM Cortex-M0/M0+/M3 (no DSP ext), WASM, and others
    #include "platforms/shared/simd_noop.hpp"  // IWYU pragma: keep
#endif
