// ok no namespace fl
#pragma once

/// @file core_detection.h
/// STM32 Arduino core differentiation and detection
///
/// This header detects which STM32 Arduino core is in use and defines appropriate
/// feature detection macros. This is critical for handling core-specific quirks,
/// such as F_CPU definitions and GPIO register access patterns.
///
/// Defines one of:
/// - FL_IS_STM32_STMDUINO: Official STMicroelectronics Arduino core (modern)
/// - FL_IS_STM32_LIBMAPLE: Roger Clark Arduino_STM32 core (legacy, deprecated)
/// - FL_IS_STM32_PARTICLE: Particle firmware (Photon/Electron)
/// - FL_IS_STM32_UNKNOWN: Unknown/unsupported core
///
/// Detection logic based on research from:
/// - https://github.com/stm32duino/Arduino_Core_STM32/issues/946
/// - https://github.com/rogerclarkmelbourne/Arduino_STM32
/// - https://community.particle.io/t/13085

// ============================================================================
// Core Detection - Mutually Exclusive
// ============================================================================

// Count how many cores are detected (for validation)
#define _FL_STM32_CORE_COUNT ( \
    (defined(ARDUINO_ARCH_STM32) ? 1 : 0) + \
    ((defined(__STM32F1__) || defined(__STM32F4__) || defined(STM32_MCU_SERIES)) ? 1 : 0) + \
    ((defined(PLATFORM_ID) || defined(SPARK)) ? 1 : 0) \
)

// STM32duino (Official STMicroelectronics Core)
// Detection: ARDUINO_ARCH_STM32 is the primary identifier
// Source: https://github.com/stm32duino/Arduino_Core_STM32/issues/946
#if defined(ARDUINO_ARCH_STM32)
  #define FL_IS_STM32_STMDUINO

  // F_CPU Behavior: Defined as SystemCoreClock (runtime variable, not compile-time constant)
  // Users must override via build_opt.h: -UF_CPU -DF_CPU=168000000UL
  // Source: https://github.com/stm32duino/Arduino_Core_STM32/issues/612

  // STM32_CORE_VERSION available since v1.5.0
  // Format: 0xAABBCCPP where AA=major, BB=minor, CC=patch, PP=sub-patch
  // Examples:
  //   0x01050000 = v1.5.0
  //   0x02030100 = v2.3.1
  //   0x02080000 = v2.8.0
  //   0x00000000 = Unknown (pre-1.5.0 or non-STM32duino core)
  #ifdef STM32_CORE_VERSION
    #define FL_STM32_VERSION STM32_CORE_VERSION
  #else
    #define FL_STM32_VERSION 0x00000000
  #endif

  // Version component extraction macros
  #define FL_STM32_VERSION_MAJOR ((FL_STM32_VERSION >> 24) & 0xFF)
  #define FL_STM32_VERSION_MINOR ((FL_STM32_VERSION >> 16) & 0xFF)
  #define FL_STM32_VERSION_PATCH ((FL_STM32_VERSION >> 8) & 0xFF)

// Arduino_STM32 (Roger Clark/Libmaple Core - Legacy, Deprecated)
// Detection: __STM32F1__, __STM32F4__ (double underscores), or STM32_MCU_SERIES
// Source: https://github.com/rogerclarkmelbourne/Arduino_STM32
// Status: "break/fix level only", "adequate for hobby use, but cannot be recommended for anything serious"
// Source: https://community.platformio.org/t/stm32-core-confusion/21430
#elif defined(__STM32F1__) || defined(__STM32F4__) || defined(STM32_MCU_SERIES)
  #define FL_IS_STM32_LIBMAPLE

  // F_CPU Behavior: May not define F_CPU, or defines as constant (varies by board)
  // Fallback values may be needed

  // STM32_MCU_SERIES values (if defined):
  // - STM32_SERIES_F1 = 0
  // - STM32_SERIES_F4 = 3

// Particle (Photon/Electron - STM32F2)
// Detection: PLATFORM_ID or SPARK
// Source: https://community.particle.io/t/13085
// Hardware: STM32F205RGY6 (STM32F2 family, 120 MHz)
#elif defined(PLATFORM_ID) || defined(SPARK)
  #define FL_IS_STM32_PARTICLE

  // F_CPU Behavior: Typically defined correctly as 120000000

// Unknown Core - Issue warning but don't fail compilation
#else
  #define FL_IS_STM32_UNKNOWN

  #warning "Unknown STM32 Arduino core detected - FastLED compatibility not guaranteed"
  #warning "Supported cores: STM32duino (ARDUINO_ARCH_STM32), Arduino_STM32 (__STM32F1__/__STM32F4__), Particle (PLATFORM_ID/SPARK)"
#endif

// ============================================================================
// Core Detection Validation - Ensure exactly one core is detected
// ============================================================================

#if _FL_STM32_CORE_COUNT > 1
  #error "Multiple STM32 Arduino cores detected - conflicting definitions. Check your build environment."
#elif _FL_STM32_CORE_COUNT == 0
  // This is handled by FL_STM32_CORE_UNKNOWN above
#endif

// Clean up temporary macro
#undef _FL_STM32_CORE_COUNT

// ============================================================================
// Core Feature Detection - F_CPU Behavior
// ============================================================================

// Does this core define F_CPU as a runtime variable (SystemCoreClock)?
#if defined(FL_IS_STM32_STMDUINO)
  #define FL_STM32_F_CPU_IS_RUNTIME_VARIABLE 1
#else
  #define FL_STM32_F_CPU_IS_RUNTIME_VARIABLE 0
#endif

// Should we expect F_CPU to be defined by the core?
#if defined(FL_IS_STM32_STMDUINO) || defined(FL_IS_STM32_PARTICLE)
  #define FL_STM32_EXPECTS_F_CPU_FROM_CORE 1
#else
  #define FL_STM32_EXPECTS_F_CPU_FROM_CORE 0
#endif
