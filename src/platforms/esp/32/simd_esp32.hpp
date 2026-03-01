// ok no namespace fl
#pragma once

// IWYU pragma: private

/// @file simd_esp32.hpp
/// ESP32 platform dispatcher for SIMD implementations
///
/// Routes to architecture-specific SIMD implementations:
/// - Xtensa (ESP32, S2, S3): simd_xtensa.hpp
/// - RISC-V (C2, C3, C5, C6, H2, P4): simd_riscv.hpp

#include "platforms/esp/is_esp.h"

#if defined(FL_IS_ESP32)

// Dispatch to architecture-specific implementation
#if defined(__XTENSA__)
    // Xtensa-based ESP32 variants (ESP32, ESP32-S2, ESP32-S3)
    #include "platforms/esp/32/simd_xtensa.hpp"
#elif defined(__riscv)
    // RISC-V-based ESP32 variants (C2, C3, C5, C6, H2, P4)
    #if defined(FL_IS_ESP_32C2) || defined(FL_IS_ESP_32C3) || \
        defined(FL_IS_ESP_32C5) || defined(FL_IS_ESP_32C6) || \
        defined(FL_IS_ESP_32H2) || defined(FL_IS_ESP_32P4)
        // Known RISC-V variants: scalar loops (no SIMD hardware), RVV-ready
        #include "platforms/esp/32/simd_riscv.hpp"
    #else
        // Unknown RISC-V variant: using scalar noop fallback
        #warning "RISC-V SIMD ops are untested on this chip, using scalar noop fallback"
        #include "platforms/shared/simd_noop.hpp"
    #endif
#else
    #error "Unknown ESP32 architecture (expected __XTENSA__ or __riscv)"
#endif

#endif  // defined(FL_IS_ESP32)
