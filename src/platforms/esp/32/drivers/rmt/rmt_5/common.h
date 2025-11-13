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


// #define FASTLED_RMT5_PRESET_LEGACY
#define FASTLED_RMT5_PRESET_BALANCED
// #define FASTLED_RMT5_PRESET_AGGRESSIVE
// #define FASTLED_RMT5_PRESET_MAX_CHANNELS
// #define FASTLED_RMT5_PRESET_AGGRESSIVE_MAX_CHANNELS


#if defined(FASTLED_RMT5_PRESET_LEGACY)
// LEGACY preset:
// - Uses RMT threshold interrupts (no timer ISR)
#define FASTLED_RMT5_USE_TIMER_ISR 0
#define FASTLED_RMT5_USE_THRESHOLD_ISR 1
#define FASTLED_RMT_MEM_BLOCKS 2

#elif defined(FASTLED_RMT5_PRESET_BALANCED)
// BALANCED preset:
// - Uses gentle RMT timer ISR with lots of memory.
// - 1 memory block per channel to maximize channel usage
// - Higher CPU load but better precision for tight timing loops
#define FASTLED_RMT5_USE_TIMER_ISR 1
#define FASTLED_RMT5_USE_THRESHOLD_ISR 0
#define FASTLED_RMT_MEM_BLOCKS 2
#define FASTLED_RMT5_TIMER_RESOLUTION_HZ 10000000  // 10 MHz = 0.1 µs per tick
#define FASTLED_RMT5_TIMER_INTERVAL_TICKS 80        // 80 ticks @10 MHz = 8.0 µs interval
#define FASTLED_RMT5_USING_PRESET

#elif defined(FASTLED_RMT5_PRESET_AGGRESSIVE)
// AGGRESSIVE preset:
// - Uses both RMT threshold interrupts and a high-rate timer ISR
// - Higher CPU load but better precision for tight timing loops
#define FASTLED_RMT5_USE_TIMER_ISR 1
#define FASTLED_RMT5_USE_THRESHOLD_ISR 1
#define FASTLED_RMT_MEM_BLOCKS 2
#define FASTLED_RMT5_TIMER_RESOLUTION_HZ 10000000  // 10 MHz = 0.1 µs per tick
#define FASTLED_RMT5_TIMER_INTERVAL_TICKS 20        // 20 ticks @10 MHz = 2.0 µs interval (aggressive; Wi-Fi may suffer)

#elif defined(FASTLED_RMT5_PRESET_MAX_CHANNELS)
// MAX CHANNELS preset:
// - Prioritizes channel count over precision
// - Uses RMT threshold interrupts + timer ISR
// - 1 memory block per channel to maximize channel usage
#define FASTLED_RMT5_USE_TIMER_ISR 1
#define FASTLED_RMT5_USE_THRESHOLD_ISR 0
#define FASTLED_RMT_MEM_BLOCKS 1
#define FASTLED_RMT5_TIMER_RESOLUTION_HZ 10000000  // 10 MHz = 0.1 µs per tick
#define FASTLED_RMT5_TIMER_INTERVAL_TICKS 80        // 80 ticks @10 MHz = 8.0 µs interval

#elif defined(FASTLED_RMT5_PRESET_AGGRESSIVE_MAX_CHANNELS)
// AGGRESSIVE MAX CHANNELS preset:
// - Prioritizes maximum channel count and minimal latency
// - Very high timer ISR rate (~0.5 µs interval) — can disrupt Wi-Fi
#define FASTLED_RMT5_USE_TIMER_ISR 1
#define FASTLED_RMT5_USE_THRESHOLD_ISR 0
#define FASTLED_RMT_MEM_BLOCKS 1
#define FASTLED_RMT5_TIMER_RESOLUTION_HZ 10000000   // 40 MHz = 0.025 µs per tick
#define FASTLED_RMT5_TIMER_INTERVAL_TICKS 20        // 20 ticks @10 MHz = 2.0 µs interval (aggressive; Wi-Fi may suffer)
#endif

#if FASTLED_RMT5_USE_TIMER_ISR == FASTLED_RMT5_USE_THRESHOLD_ISR
#error "FASTLED_RMT5_USE_TIMER_ISR and FASTLED_RMT5_USE_THRESHOLD_ISR cannot both be enabled"
#endif

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
#define FASTLED_RMT5_TIMER_RESOLUTION_HZ 10000000  // 10MHz = 0.1µs tick resolution (max)
#endif

#ifndef FASTLED_RMT5_TIMER_INTERVAL_TICKS
#define FASTLED_RMT5_TIMER_INTERVAL_TICKS 20  // Fire every 5 ticks = 2.0µs at 10MHz
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
// Riscv chipsets play nice with higher level interrupts + c isr handlers.
#define FL_RMT5_INTERRUPT_LEVEL ESP_INTR_FLAG_LEVEL6  // More resitant to jitter.
#else
// Xtensa sometimes need asm isr handlers, level 3 is safe.
#define FL_RMT5_INTERRUPT_LEVEL ESP_INTR_FLAG_LEVEL3  // Test if this can go higher without asm handler.
#endif


#endif // ESP32
