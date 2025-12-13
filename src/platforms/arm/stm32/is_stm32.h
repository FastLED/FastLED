// ok no namespace fl
#pragma once

/// @file is_stm32.h
/// STM32 platform detection header
///
/// This header detects STM32-based platforms by checking compiler-defined macros
/// and defines FL_IS_STM32 when an STM32 platform is detected.
///
/// Defines:
/// - FL_IS_STM32: General STM32 platform (any family)
/// - FL_IS_STM32_F1: STM32F1 family (Blue Pill, etc.)
/// - FL_IS_STM32_F2: STM32F2 family (Particle Photon)
/// - FL_IS_STM32_F4: STM32F4 family (Black Pill, etc.)
/// - FL_IS_STM32_F7: STM32F7 family
/// - FL_IS_STM32_L4: STM32L4 family (low-power)
/// - FL_IS_STM32_H7: STM32H7 family (Giga R1, high-performance)
/// - FL_IS_STM32_G4: STM32G4 family (motor control)
/// - FL_IS_STM32_U5: STM32U5 family (ultra-low-power)
///
/// Used by platforms/arm/is_arm.h for platform dispatching and by STM32 platform
/// headers for validation that STM32 detection has occurred.
///
/// These macros are defined by various STM32 toolchains:
/// 1. STM32duino (Official STMicroelectronics core): STM32F1xx, STM32F4, etc.
/// 2. Roger Clark STM32 (STM32F103C): __STM32F1__
/// 3. Older STM32duino/Arduino_STM32: STM32F1, STM32F4
/// 4. Particle (Photon): STM32F10X_MD, STM32F2XX

// ============================================================================
// FL_IS_STM32 - General STM32 platform detection
// ============================================================================
#if \
    /* STM32F1 Family (72 MHz, 20-64 KB RAM) */ \
    defined(STM32F10X_MD) || defined(__STM32F1__) || defined(STM32F1) || defined(STM32F1xx) || \
    /* STM32F2 Family (120 MHz, 64-128 KB RAM) */ \
    defined(STM32F2XX) || defined(STM32F2xx) || \
    /* STM32F4 Family (84-180 MHz, 64-256 KB RAM) */ \
    defined(STM32F4) || defined(STM32F4xx) || \
    /* STM32F7 Family (216 MHz, 256-512 KB RAM) */ \
    defined(STM32F7) || defined(STM32F7xx) || \
    /* STM32L4 Family (80 MHz, Low Power) */ \
    defined(STM32L4) || defined(STM32L4xx) || \
    /* STM32H7 Family (400-480 MHz, High Performance) */ \
    defined(STM32H7) || defined(STM32H7xx) || \
    defined(STM32H743xx) || defined(STM32H747xx) || defined(STM32H750xx) || \
    defined(STM32H723xx) || defined(STM32H725xx) || defined(STM32H730xx) || \
    defined(STM32H735xx) || defined(STM32H755xx) || defined(STM32H757xx) || \
    defined(STM32H7A3xx) || defined(STM32H7B3xx) || \
    /* STM32G4 Family (170 MHz, Motor Control) */ \
    defined(STM32G4) || defined(STM32G4xx) || \
    /* STM32U5 Family (160 MHz, Ultra Low Power) */ \
    defined(STM32U5) || defined(STM32U5xx)
#define FL_IS_STM32
#endif

// ============================================================================
// STM32 Family Variant Detection - Specific chip family identification
// ============================================================================

// FL_IS_STM32_F1 - STM32F1 Family (72 MHz, 20-64 KB RAM, 64-512 KB Flash)
#if defined(STM32F10X_MD) || defined(__STM32F1__) || defined(STM32F1) || defined(STM32F1xx)
#define FL_IS_STM32_F1
#endif

// FL_IS_STM32_F2 - STM32F2 Family (120 MHz, 64-128 KB RAM, 256-1024 KB Flash)
#if defined(STM32F2XX) || defined(STM32F2xx)
#define FL_IS_STM32_F2
#endif

// FL_IS_STM32_F4 - STM32F4 Family (84-180 MHz, 64-256 KB RAM, 256-2048 KB Flash)
#if defined(STM32F4) || defined(STM32F4xx)
#define FL_IS_STM32_F4
#endif

// FL_IS_STM32_F7 - STM32F7 Family (216 MHz, 256-512 KB RAM, 512-2048 KB Flash)
#if defined(STM32F7) || defined(STM32F7xx)
#define FL_IS_STM32_F7
#endif

// FL_IS_STM32_L4 - STM32L4 Family (80 MHz, 64-640 KB RAM, 256-1024 KB Flash, Low-power)
#if defined(STM32L4) || defined(STM32L4xx)
#define FL_IS_STM32_L4
#endif

// FL_IS_STM32_H7 - STM32H7 Family (400-480 MHz, 128-1024 KB RAM, 1-2 MB Flash, High-performance)
#if defined(STM32H7) || defined(STM32H7xx) || \
    defined(STM32H743xx) || defined(STM32H747xx) || defined(STM32H750xx) || \
    defined(STM32H723xx) || defined(STM32H725xx) || defined(STM32H730xx) || \
    defined(STM32H735xx) || defined(STM32H755xx) || defined(STM32H757xx) || \
    defined(STM32H7A3xx) || defined(STM32H7B3xx)
#define FL_IS_STM32_H7
#endif

// FL_IS_STM32_G4 - STM32G4 Family (170 MHz, 32-128 KB RAM, 128-512 KB Flash, Mainstream)
#if defined(STM32G4) || defined(STM32G4xx)
#define FL_IS_STM32_G4
#endif

// FL_IS_STM32_U5 - STM32U5 Family (160 MHz, 256-2496 KB RAM, 512-4096 KB Flash, Ultra-low-power)
#if defined(STM32U5) || defined(STM32U5xx)
#define FL_IS_STM32_U5
#endif
