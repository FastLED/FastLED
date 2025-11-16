#pragma once

// ok no namespace fl

#include "third_party/espressif/led_strip/src/enabled.h"

#if FASTLED_RMT5

#include "fl/compiler_control.h"
#include "ftl/stdint.h"
#include "sdkconfig.h"

FL_EXTERN_C_BEGIN
#include "soc/rmt_struct.h"
#include "esp_attr.h"
FL_EXTERN_C_END



//=============================================================================
// RMT5 Common Definitions and Hardware Abstraction Macros
//=============================================================================

// RMT clock frequency configuration
// This can be overridden before including FastLED.h if different frequency is needed
#ifndef FASTLED_RMT5_CLOCK_HZ
#define FASTLED_RMT5_CLOCK_HZ 40000000  // 40MHz (25ns resolution), same as NeoPixelBus.
#endif

// RMT memory configuration (matching RMT4)
#ifndef FASTLED_RMT_MEM_WORDS_PER_CHANNEL
#define FASTLED_RMT_MEM_WORDS_PER_CHANNEL SOC_RMT_MEM_WORDS_PER_CHANNEL
#endif

#ifndef FASTLED_RMT_MEM_BLOCKS
#define FASTLED_RMT_MEM_BLOCKS 2  // 2x as much memory for increased stability vs wifi jitter.
#endif

// Buffer size calculations
#define FASTLED_RMT5_MAX_PULSES (FASTLED_RMT_MEM_WORDS_PER_CHANNEL * FASTLED_RMT_MEM_BLOCKS)
#define FASTLED_RMT5_PULSES_PER_FILL (FASTLED_RMT5_MAX_PULSES / FASTLED_RMT_MEM_BLOCKS)  // Half buffer

// DMA configuration
// ⚠️ WARNING: DMA has significant limitations and risks on ESP32-S3:
//   1. HARDWARE LIMIT: Only ONE RMT channel can use DMA simultaneously
//   2. SYSTEM CONTENTION: DMA channels are shared with SPI, I2S, UART, ADC/DAC
//      - Enabling DMA for RMT may starve other peripherals
//
// DMA Buffer Sizing:
// - With DMA: Full strip size (num_bytes * 8 symbols + 16) - zero WiFi flicker!
// - Without DMA: Double-buffering with FASTLED_RMT5_MAX_PULSES symbols
//
// OPTIMIZATION: Dynamic memory allocation based on channel count
// ESP32-S3 example (4 channels × 48 words = 192 total):
// - Single strip (no DMA): Gets ALL 192 words = QUAD-BUFFERING (maximum WiFi resistance)
// - Dual strips (no DMA): Each gets 96 words = double-buffering (2 channels max)
// - With 1 DMA + 1 non-DMA: DMA gets full strip buffer, non-DMA gets 192 words = QUAD-BUFFERING
// - Result: Optimal memory allocation for any configuration!
//
// For WiFi robustness with multiple strips, raise interrupt priority instead!
#ifndef FASTLED_RMT5_USE_DMA
    // ESP32-S3: Enable DMA by default for maximum WiFi immunity
    // Other platforms: Disabled (may not support or have limitations)
    #if defined(CONFIG_IDF_TARGET_ESP32S3)
        #define FASTLED_RMT5_USE_DMA 1  // Default: enabled on ESP32-S3
    #else
        #define FASTLED_RMT5_USE_DMA 0  // Default: disabled on other platforms
    #endif
#endif


// === Platform-Specific Signal Routing ===

// RMT signal routing for GPIO matrix
#if defined(CONFIG_IDF_TARGET_ESP32P4)
#define RMT_SIG_PAD_IDX RMT_SIG_PAD_OUT0_IDX
#else
#define RMT_SIG_PAD_IDX RMT_SIG_OUT0_IDX
#endif

// Note that WIFI interrupt = level 4.
#if defined(CONFIG_IDF_TARGET_ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32C5)
// Rumor is this can go higher without asm handler.
#define FL_RMT5_INTERRUPT_LEVEL ESP_INTR_FLAG_LEVEL3
#else
// Xtensa might need an asm isr handlers, level 3 is safe.
#define FL_RMT5_INTERRUPT_LEVEL ESP_INTR_FLAG_LEVEL3
#endif


#endif // ESP32
