#pragma once

// ok no namespace fl


#ifdef ESP32

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

// === Configuration Constants ===

// RMT clock frequency configuration
// This can be overridden before including FastLED.h if different frequency is needed
#ifndef FASTLED_RMT5_CLOCK_HZ
#define FASTLED_RMT5_CLOCK_HZ 40000000  // 40MHz (25ns resolution)
#endif

// RMT memory configuration (matching RMT4)
#ifndef FASTLED_RMT_MEM_WORDS_PER_CHANNEL
#define FASTLED_RMT_MEM_WORDS_PER_CHANNEL SOC_RMT_MEM_WORDS_PER_CHANNEL
#endif

#ifndef FASTLED_RMT_MEM_BLOCKS
#define FASTLED_RMT_MEM_BLOCKS 2  // Ping-pong buffer by default
#endif

// Buffer size calculations
#define FASTLED_RMT5_MAX_PULSES (FASTLED_RMT_MEM_WORDS_PER_CHANNEL * FASTLED_RMT_MEM_BLOCKS)
#define FASTLED_RMT5_PULSES_PER_FILL (FASTLED_RMT5_MAX_PULSES / FASTLED_RMT_MEM_BLOCKS)  // Half buffer

// Interrupt mode selection
// 0 = RMT threshold interrupts (original, less aggressive, lower CPU overhead)
// 1 = Timer-driven interrupts (new, sub-microsecond filling, nibble-level granularity, higher CPU)
#ifndef FASTLED_RMT5_USE_TIMER_ISR
#define FASTLED_RMT5_USE_TIMER_ISR 0  // Default to threshold mode
#endif

// Timer interrupt configuration for aggressive buffer filling
// Timer drives fills at sub-microsecond intervals instead of using RMT threshold interrupts
// Note: Always defined to support both threshold and timer variants being compiled
#ifndef FASTLED_RMT5_TIMER_GROUP
#define FASTLED_RMT5_TIMER_GROUP 1  // Timer Group 0 (0 or 1)
#endif

#ifndef FASTLED_RMT5_TIMER_INDEX
#define FASTLED_RMT5_TIMER_INDEX 1  // Timer 1 (0 or 1) - TG0_T0 often used by FreeRTOS
#endif

#ifndef FASTLED_RMT5_TIMER_RESOLUTION_HZ
#define FASTLED_RMT5_TIMER_RESOLUTION_HZ 10000000  // 10MHz = 0.1µs tick resolution
#endif

#ifndef FASTLED_RMT5_TIMER_INTERVAL_TICKS
#define FASTLED_RMT5_TIMER_INTERVAL_TICKS 5  // Fire every 5 ticks = 0.5µs at 10MHz
#endif

// === Platform-Specific Signal Routing ===

// RMT signal routing for GPIO matrix
#if defined(CONFIG_IDF_TARGET_ESP32P4)
#define RMT_SIG_PAD_IDX RMT_SIG_PAD_OUT0_IDX
#else
#define RMT_SIG_PAD_IDX RMT_SIG_OUT0_IDX
#endif

#if defined(CONFIG_IDF_TARGET_ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32C5)
#define FL_RMT5_INTERRUPT_LEVEL ESP_INTR_FLAG_LEVEL6  // More resitant to jitter.
#else
#define FL_RMT5_INTERRUPT_LEVEL ESP_INTR_FLAG_LEVEL3  // Test if this can go higher.
#endif


#endif // ESP32
