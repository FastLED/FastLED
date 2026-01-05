// ok no namespace fl
#pragma once

/// @file simd_esp32.hpp
/// ESP32 platform dispatcher for SIMD implementations
///
/// Routes to architecture-specific SIMD implementations:
/// - Xtensa (ESP32, S2, S3): simd_xtensa.hpp
/// - RISC-V (C2, C3, C5, C6, H2, P4): simd_riscv.hpp

#if defined(ESP32)

// Dispatch to architecture-specific implementation
#if defined(__XTENSA__)
    // Xtensa-based ESP32 variants (ESP32, ESP32-S2, ESP32-S3)
    #include "platforms/esp/32/simd_xtensa.hpp"
#elif defined(__riscv)
    // RISC-V-based ESP32 variants (C2, C3, C5, C6, H2, P4)
    #include "platforms/esp/32/simd_riscv.hpp"
#else
    #error "Unknown ESP32 architecture (expected __XTENSA__ or __riscv)"
#endif

#endif  // defined(ESP32)
