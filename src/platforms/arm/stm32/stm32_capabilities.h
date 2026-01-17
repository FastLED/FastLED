// ok no namespace fl

/// @file stm32_capabilities.h
/// @brief STM32 capability and feature detection macros
///
/// This header provides STM32 capability flags and feature detection for
/// conditional compilation in STM32 driver implementations.
///
/// IMPORTANT: This header requires is_stm32.h to be included first for
/// family detection (FL_IS_STM32_F1, FL_IS_STM32_F4, etc.)
///
/// Usage: Include this file in STM32 driver implementations to access:
/// - DMA architecture (FASTLED_STM32_DMA_CHANNEL_BASED vs FASTLED_STM32_DMA_STREAM_BASED)
/// - Capability flags (FASTLED_STM32_HAS_DMAMUX, FASTLED_STM32_HAS_TIM5, etc.)
/// - Conditional HAL/LL header includes
/// - Platform capability constants (max frequencies, channel counts, etc.)
///
/// Created: STM32 Capability Detection Macros
/// Version: 1.0

#pragma once

// ============================================================================
// Platform Detection Guard - Only include on STM32 platforms
// ============================================================================

#include "is_stm32.h"

#ifndef FL_IS_STM32
    #error "stm32_capabilities.h included on non-STM32 platform"
#endif

// ============================================================================
// STM32 Family Name String Constants
// ============================================================================
// These are string literals, not platform detection macros

#if defined(FL_IS_STM32_F1)
    #define FASTLED_STM32_FAMILY_NAME "STM32F1"
#elif defined(FL_IS_STM32_F2)
    #define FASTLED_STM32_FAMILY_NAME "STM32F2"
#elif defined(FL_IS_STM32_F4)
    #define FASTLED_STM32_FAMILY_NAME "STM32F4"
#elif defined(FL_IS_STM32_F7)
    #define FASTLED_STM32_FAMILY_NAME "STM32F7"
#elif defined(FL_IS_STM32_L4)
    #define FASTLED_STM32_FAMILY_NAME "STM32L4"
#elif defined(FL_IS_STM32_H7)
    #define FASTLED_STM32_FAMILY_NAME "STM32H7"
#elif defined(FL_IS_STM32_G4)
    #define FASTLED_STM32_FAMILY_NAME "STM32G4"
#elif defined(FL_IS_STM32_U5)
    #define FASTLED_STM32_FAMILY_NAME "STM32U5"
#else
    #define FASTLED_STM32_FAMILY_NAME "STM32_Unknown"
    #warning "Unknown STM32 family - some features may not work correctly"
#endif

// ============================================================================
// DMA Architecture Detection
// ============================================================================

// STM32F1 uses channel-based DMA (DMA1/DMA2, 7 channels each)
#if defined(FL_IS_STM32_F1)
    #define FASTLED_STM32_DMA_CHANNEL_BASED
    #define FASTLED_STM32_DMA_CONTROLLERS 2
    #define FASTLED_STM32_DMA_CHANNELS_PER_CONTROLLER 7
    #define FASTLED_STM32_DMA_TOTAL_CHANNELS 14
#endif

// STM32F2/F4/F7/L4 use stream-based DMA (DMA1/DMA2, 8 streams each, multiplexed channels)
#if defined(FL_IS_STM32_F2) || defined(FL_IS_STM32_F4) || \
    defined(FL_IS_STM32_F7) || defined(FL_IS_STM32_L4)
    #define FASTLED_STM32_DMA_STREAM_BASED
    #define FASTLED_STM32_DMA_CONTROLLERS 2
    #define FASTLED_STM32_DMA_STREAMS_PER_CONTROLLER 8
    #define FASTLED_STM32_DMA_TOTAL_STREAMS 16
    // F4/F7/L4 have 8 possible channel numbers (0-7) per stream
    #define FASTLED_STM32_DMA_CHANNEL_COUNT 8
#endif

// STM32H7 uses stream-based DMA with BDMA and MDMA variants
#if defined(FL_IS_STM32_H7)
    #define FASTLED_STM32_DMA_STREAM_BASED
    #define FASTLED_STM32_DMA_CONTROLLERS 2  // DMA1 and DMA2
    #define FASTLED_STM32_DMA_STREAMS_PER_CONTROLLER 8
    #define FASTLED_STM32_DMA_TOTAL_STREAMS 16
    #define FASTLED_STM32_HAS_DMAMUX  // H7 has DMAMUX for flexible routing
    #define FASTLED_STM32_HAS_BDMA    // H7 has BDMA for low-power domain
    #define FASTLED_STM32_HAS_MDMA    // H7 has MDMA for memory-to-memory
#endif

// STM32G4 uses DMA with DMAMUX
#if defined(FL_IS_STM32_G4)
    #define FASTLED_STM32_DMA_CHANNEL_BASED  // G4 uses channel terminology
    #define FASTLED_STM32_DMA_CONTROLLERS 2
    #define FASTLED_STM32_DMA_CHANNELS_PER_CONTROLLER 8
    #define FASTLED_STM32_DMA_TOTAL_CHANNELS 16
    #define FASTLED_STM32_HAS_DMAMUX  // G4 has DMAMUX for flexible routing
#endif

// STM32U5 uses GPDMA (General Purpose DMA) with advanced features
#if defined(FL_IS_STM32_U5)
    #define FASTLED_STM32_DMA_GPDMA_BASED  // U5 uses new GPDMA architecture
    #define FASTLED_STM32_DMA_CONTROLLERS 4  // GPDMA1, LPDMA1, etc.
    #define FASTLED_STM32_DMA_CHANNELS_PER_CONTROLLER 16
    #define FASTLED_STM32_DMA_TOTAL_CHANNELS 64
    #define FASTLED_STM32_HAS_DMAMUX  // U5 has flexible DMA routing
#endif

// Convenience macro for code that uses DMA streams (F2/F4/F7/H7)
#if defined(FASTLED_STM32_DMA_STREAM_BASED)
    #define FASTLED_STM32_HAS_DMA_STREAMS
#endif

// ============================================================================
// Timer Peripheral Capability Flags
// ============================================================================

// All STM32 families have TIM2, TIM3, TIM4
#define FASTLED_STM32_HAS_TIM2
#define FASTLED_STM32_HAS_TIM3
#define FASTLED_STM32_HAS_TIM4

// TIM5 is 32-bit timer available on F2/F4/F7/H7
#if defined(FL_IS_STM32_F2) || defined(FL_IS_STM32_F4) || \
    defined(FL_IS_STM32_F7) || defined(FL_IS_STM32_H7)
    #define FASTLED_STM32_HAS_TIM5
#endif

// TIM8 is advanced timer available on most families (check specific variants)
#if defined(FL_IS_STM32_F1) || defined(FL_IS_STM32_F2) || defined(FL_IS_STM32_F4) || \
    defined(FL_IS_STM32_F7) || defined(FL_IS_STM32_H7) || defined(FL_IS_STM32_G4)
    #define FASTLED_STM32_HAS_TIM8
#endif

// TIM15/TIM16/TIM17 available on H7, G4, L4, U5
#if defined(FL_IS_STM32_H7) || defined(FL_IS_STM32_G4) || \
    defined(FL_IS_STM32_L4) || defined(FL_IS_STM32_U5)
    #define FASTLED_STM32_HAS_TIM15
    #define FASTLED_STM32_HAS_TIM16
    #define FASTLED_STM32_HAS_TIM17
#endif

// ============================================================================
// GPIO Capability Flags
// ============================================================================

// GPIO maximum toggle frequency (approximate)
#if defined(FL_IS_STM32_F1)
    #define FASTLED_STM32_GPIO_MAX_FREQ_MHZ 50
#elif defined(FL_IS_STM32_F2)
    #define FASTLED_STM32_GPIO_MAX_FREQ_MHZ 60
#elif defined(FL_IS_STM32_F4) || defined(FL_IS_STM32_F7) || defined(FL_IS_STM32_H7)
    #define FASTLED_STM32_GPIO_MAX_FREQ_MHZ 100
#elif defined(FL_IS_STM32_L4)
    #define FASTLED_STM32_GPIO_MAX_FREQ_MHZ 80
#elif defined(FL_IS_STM32_G4)
    #define FASTLED_STM32_GPIO_MAX_FREQ_MHZ 170
#elif defined(FL_IS_STM32_U5)
    #define FASTLED_STM32_GPIO_MAX_FREQ_MHZ 160
#else
    #define FASTLED_STM32_GPIO_MAX_FREQ_MHZ 50  // Conservative default
#endif

// F1 uses AFIO remapping instead of AF numbers
#if defined(FL_IS_STM32_F1)
    #define FASTLED_STM32_GPIO_USES_AFIO_REMAP
#else
    #define FASTLED_STM32_GPIO_USES_AF_NUMBERS
#endif

// GPIO speed compatibility macro (F1 doesn't have VERY_HIGH speed)
#if defined(FL_IS_STM32_F1)
    // F1 only has LOW, MEDIUM, HIGH speeds
    #define FASTLED_GPIO_SPEED_MAX GPIO_SPEED_FREQ_HIGH
#else
    // F4/F7/H7 and others have VERY_HIGH speed
    #define FASTLED_GPIO_SPEED_MAX GPIO_SPEED_FREQ_VERY_HIGH
#endif

// ============================================================================
// STM32 HAL/LL Header Includes
// ============================================================================
#if __has_include("stm32_def.h")
    #include <stm32_def.h>

    // Family-specific LL (Low-Layer) driver includes for performance-critical code
    #if defined(FL_IS_STM32_F1)
        // F1 LL driver headers
        #if __has_include("stm32f1xx_ll_tim.h")
            #include "stm32f1xx_ll_tim.h"
        #endif
        #if __has_include("stm32f1xx_ll_dma.h")
            #include "stm32f1xx_ll_dma.h"
        #endif
        #if __has_include("stm32f1xx_ll_gpio.h")
            #include "stm32f1xx_ll_gpio.h"
        #endif
        #if __has_include("stm32f1xx_ll_bus.h")
            #include "stm32f1xx_ll_bus.h"
        #endif

    #elif defined(FL_IS_STM32_F2)
        // F2 LL driver headers
        #if __has_include("stm32f2xx_ll_tim.h")
            #include "stm32f2xx_ll_tim.h"
        #endif
        #if __has_include("stm32f2xx_ll_dma.h")
            #include "stm32f2xx_ll_dma.h"
        #endif
        #if __has_include("stm32f2xx_ll_gpio.h")
            #include "stm32f2xx_ll_gpio.h"
        #endif
        #if __has_include("stm32f2xx_ll_bus.h")
            #include "stm32f2xx_ll_bus.h"
        #endif

    #elif defined(FL_IS_STM32_F4)
        // F4 LL driver headers
        #if __has_include("stm32f4xx_ll_tim.h")
            #include "stm32f4xx_ll_tim.h"
        #endif
        #if __has_include("stm32f4xx_ll_dma.h")
            #include "stm32f4xx_ll_dma.h"
        #endif
        #if __has_include("stm32f4xx_ll_gpio.h")
            #include "stm32f4xx_ll_gpio.h"
        #endif
        #if __has_include("stm32f4xx_ll_bus.h")
            #include "stm32f4xx_ll_bus.h"
        #endif

    #elif defined(FL_IS_STM32_F7)
        // F7 LL driver headers
        #if __has_include("stm32f7xx_ll_tim.h")
            #include "stm32f7xx_ll_tim.h"
        #endif
        #if __has_include("stm32f7xx_ll_dma.h")
            #include "stm32f7xx_ll_dma.h"
        #endif
        #if __has_include("stm32f7xx_ll_gpio.h")
            #include "stm32f7xx_ll_gpio.h"
        #endif
        #if __has_include("stm32f7xx_ll_bus.h")
            #include "stm32f7xx_ll_bus.h"
        #endif

    #elif defined(FL_IS_STM32_L4)
        // L4 LL driver headers
        #if __has_include("stm32l4xx_ll_tim.h")
            #include "stm32l4xx_ll_tim.h"
        #endif
        #if __has_include("stm32l4xx_ll_dma.h")
            #include "stm32l4xx_ll_dma.h"
        #endif
        #if __has_include("stm32l4xx_ll_gpio.h")
            #include "stm32l4xx_ll_gpio.h"
        #endif
        #if __has_include("stm32l4xx_ll_bus.h")
            #include "stm32l4xx_ll_bus.h"
        #endif
        // L4 also needs DMAMUX
        #if defined(FASTLED_STM32_HAS_DMAMUX)
            #if __has_include("stm32l4xx_ll_dmamux.h")
                #include "stm32l4xx_ll_dmamux.h"
            #endif
        #endif

    #elif defined(FL_IS_STM32_H7)
        // H7 LL driver headers
        #if __has_include("stm32h7xx_ll_tim.h")
            #include "stm32h7xx_ll_tim.h"
        #endif
        #if __has_include("stm32h7xx_ll_dma.h")
            #include "stm32h7xx_ll_dma.h"
        #endif
        #if __has_include("stm32h7xx_ll_gpio.h")
            #include "stm32h7xx_ll_gpio.h"
        #endif
        #if __has_include("stm32h7xx_ll_bus.h")
            #include "stm32h7xx_ll_bus.h"
        #endif
        // H7 has DMAMUX
        #if defined(FASTLED_STM32_HAS_DMAMUX)
            #if __has_include("stm32h7xx_ll_dmamux.h")
                #include "stm32h7xx_ll_dmamux.h"
            #endif
        #endif
        // H7 has BDMA
        #if defined(FASTLED_STM32_HAS_BDMA)
            #if __has_include("stm32h7xx_ll_bdma.h")
                #include "stm32h7xx_ll_bdma.h"
            #endif
        #endif
        // H7 has MDMA
        #if defined(FASTLED_STM32_HAS_MDMA)
            #if __has_include("stm32h7xx_ll_mdma.h")
                #include "stm32h7xx_ll_mdma.h"
            #endif
        #endif

    #elif defined(FL_IS_STM32_G4)
        // G4 LL driver headers
        #if __has_include("stm32g4xx_ll_tim.h")
            #include "stm32g4xx_ll_tim.h"
        #endif
        #if __has_include("stm32g4xx_ll_dma.h")
            #include "stm32g4xx_ll_dma.h"
        #endif
        #if __has_include("stm32g4xx_ll_gpio.h")
            #include "stm32g4xx_ll_gpio.h"
        #endif
        #if __has_include("stm32g4xx_ll_bus.h")
            #include "stm32g4xx_ll_bus.h"
        #endif
        // G4 has DMAMUX
        #if defined(FASTLED_STM32_HAS_DMAMUX)
            #if __has_include("stm32g4xx_ll_dmamux.h")
                #include "stm32g4xx_ll_dmamux.h"
            #endif
        #endif

    #elif defined(FL_IS_STM32_U5)
        // U5 LL driver headers (GPDMA-based)
        #if __has_include("stm32u5xx_ll_tim.h")
            #include "stm32u5xx_ll_tim.h"
        #endif
        #if __has_include("stm32u5xx_ll_dma.h")
            #include "stm32u5xx_ll_dma.h"
        #endif
        #if __has_include("stm32u5xx_ll_gpio.h")
            #include "stm32u5xx_ll_gpio.h"
        #endif
        #if __has_include("stm32u5xx_ll_bus.h")
            #include "stm32u5xx_ll_bus.h"
        #endif
    #endif

#else
    // stm32_def.h not found - STM32duino HAL not available
    // This is expected when using Arduino Mbed framework
    // Hardware SPI features will be disabled, falling back to software bitbang
    #if !defined(ARDUINO_ARCH_MBED)
        #warning "STM32 HAL not available (stm32_def.h not found) - hardware SPI disabled"
    #endif
#endif

// ============================================================================
// Platform Capability Summary Macros
// ============================================================================

// Maximum number of parallel SPI buses supported on this platform
#if defined(FL_IS_STM32_F1)
    // F1 limited by 14 DMA channels
    #define FASTLED_STM32_MAX_DUAL_SPI_BUSES 3   // 2 channels each = 6 total
    #define FASTLED_STM32_MAX_QUAD_SPI_BUSES 2   // 4 channels each = 8 total
    #define FASTLED_STM32_MAX_OCTAL_SPI_BUSES 1  // 8 channels each = 8 total (not recommended)
#elif defined(FL_IS_STM32_F2) || defined(FL_IS_STM32_F4) || \
      defined(FL_IS_STM32_F7) || defined(FL_IS_STM32_L4) || defined(FL_IS_STM32_G4)
    // F2/F4/F7/L4/G4 have 16 DMA channels/streams
    #define FASTLED_STM32_MAX_DUAL_SPI_BUSES 4   // 2 streams each = 8 total
    #define FASTLED_STM32_MAX_QUAD_SPI_BUSES 2   // 4 streams each = 8 total
    #define FASTLED_STM32_MAX_OCTAL_SPI_BUSES 2  // 8 streams each = 16 total
#elif defined(FL_IS_STM32_H7)
    // H7 has 16 DMA streams + DMAMUX for flexible routing
    #define FASTLED_STM32_MAX_DUAL_SPI_BUSES 8   // 2 streams each = 16 total
    #define FASTLED_STM32_MAX_QUAD_SPI_BUSES 4   // 4 streams each = 16 total
    #define FASTLED_STM32_MAX_OCTAL_SPI_BUSES 2  // 8 streams each = 16 total
#elif defined(FL_IS_STM32_U5)
    // U5 has 64 GPDMA channels - extremely flexible
    #define FASTLED_STM32_MAX_DUAL_SPI_BUSES 16  // 2 channels each = 32 total
    #define FASTLED_STM32_MAX_QUAD_SPI_BUSES 8   // 4 channels each = 32 total
    #define FASTLED_STM32_MAX_OCTAL_SPI_BUSES 4  // 8 channels each = 32 total
#else
    // Conservative defaults for unknown platforms
    #define FASTLED_STM32_MAX_DUAL_SPI_BUSES 2
    #define FASTLED_STM32_MAX_QUAD_SPI_BUSES 1
    #define FASTLED_STM32_MAX_OCTAL_SPI_BUSES 1
#endif

// ============================================================================
// Helper Macros for Conditional Compilation
// ============================================================================

// Check if we're using channel-based DMA (F1, G4)
#if defined(FASTLED_STM32_DMA_CHANNEL_BASED)
    #define FASTLED_STM32_DMA_IS_CHANNEL_BASED
    #define FASTLED_STM32_DMA_IS_STREAM_BASED 0
#elif defined(FASTLED_STM32_DMA_STREAM_BASED)
    #define FASTLED_STM32_DMA_IS_CHANNEL_BASED 0
    #define FASTLED_STM32_DMA_IS_STREAM_BASED
#else
    #define FASTLED_STM32_DMA_IS_CHANNEL_BASED 0
    #define FASTLED_STM32_DMA_IS_STREAM_BASED 0
#endif

// Check if DMAMUX is available for flexible DMA routing
#if defined(FASTLED_STM32_HAS_DMAMUX)
    #define FASTLED_STM32_SUPPORTS_FLEXIBLE_DMA_ROUTING
#else
    #define FASTLED_STM32_SUPPORTS_FLEXIBLE_DMA_ROUTING 0
#endif

// ============================================================================
// Validation and Debug Information
// ============================================================================

// Compile-time validation that exactly one family is detected
#if !defined(FL_IS_STM32_F1) && !defined(FL_IS_STM32_F2) && \
    !defined(FL_IS_STM32_F4) && !defined(FL_IS_STM32_F7) && \
    !defined(FL_IS_STM32_L4) && !defined(FL_IS_STM32_H7) && \
    !defined(FL_IS_STM32_G4) && !defined(FL_IS_STM32_U5)
    #warning "No specific STM32 family detected - using conservative defaults"
#endif

// Debug: Print detected configuration (can be enabled for troubleshooting)
#ifdef FASTLED_STM32_PLATFORM_DEBUG
    #pragma message "FastLED STM32 Platform Detection:"
    #pragma message "  Family: " FASTLED_STM32_FAMILY_NAME
    #if defined(FASTLED_STM32_DMA_CHANNEL_BASED)
        #pragma message "  DMA Architecture: Channel-based"
    #elif defined(FASTLED_STM32_DMA_STREAM_BASED)
        #pragma message "  DMA Architecture: Stream-based"
    #elif defined(FASTLED_STM32_DMA_GPDMA_BASED)
        #pragma message "  DMA Architecture: GPDMA-based"
    #endif
    #if defined(FASTLED_STM32_HAS_DMAMUX)
        #pragma message "  DMAMUX: Available"
    #endif
#endif

// ============================================================================
// Hardware SPI Support Detection
// ============================================================================
// These flags control whether hardware-accelerated parallel SPI is available.
// If undefined, the weak binding in spi_hw_*.cpp will return an empty vector,
// causing FastLED to fall back to software bitbang implementation.

// IMPORTANT: Hardware SPI implementation requires STM32duino HAL/LL drivers.
// The Arduino Mbed framework does NOT provide compatible HAL/LL drivers.
// Disable hardware SPI when using Mbed framework.
#if !defined(ARDUINO_ARCH_MBED)

// Dual-SPI (2 parallel data lanes) support
// Currently implemented only for stream-based DMA platforms (F2/F4/F7/H7/L4)
// For channel-based platforms (F1, G4), implementation is not yet available
#if defined(FASTLED_STM32_DMA_STREAM_BASED)
    #define FL_STM32_HAS_SPI_HW_2
#endif
// Note: Uncomment when channel-based DMA implementation is added:
// #if defined(FASTLED_STM32_DMA_CHANNEL_BASED)
//     #define FL_STM32_HAS_SPI_HW_2
// #endif

// Quad-SPI (4 parallel data lanes) support
// Currently implemented only for stream-based DMA platforms (F2/F4/F7/H7/L4)
// For channel-based platforms (F1, G4), implementation is not yet available
#if defined(FASTLED_STM32_DMA_STREAM_BASED)
    #define FL_STM32_HAS_SPI_HW_4
#endif
// Note: Uncomment when channel-based DMA implementation is added:
// #if defined(FASTLED_STM32_DMA_CHANNEL_BASED)
//     #define FL_STM32_HAS_SPI_HW_4
// #endif

// Octal-SPI (8 parallel data lanes) support
// Currently implemented only for stream-based DMA platforms (F2/F4/F7/H7/L4)
// For channel-based platforms (F1, G4), implementation is not yet available
#if defined(FASTLED_STM32_DMA_STREAM_BASED)
    #define FL_STM32_HAS_SPI_HW_8
#endif
// Note: Uncomment when channel-based DMA implementation is added:
// #if defined(FASTLED_STM32_DMA_CHANNEL_BASED)
//     #define FL_STM32_HAS_SPI_HW_8
// #endif

#else
    // Mbed framework detected - hardware SPI not supported
    // FastLED will fall back to software bitbang implementation
    #pragma message "FastLED: Mbed framework detected - hardware SPI disabled (using software bitbang)"
#endif
