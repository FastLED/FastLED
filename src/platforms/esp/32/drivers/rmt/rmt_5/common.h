#pragma once

// ok no namespace fl

#include "platforms/esp/32/feature_flags/enabled.h"

#if FASTLED_RMT5

#include "fl/compiler_control.h"
#include "fl/stl/stdint.h"
#include "sdkconfig.h"

FL_EXTERN_C_BEGIN
#include "soc/rmt_struct.h"
#include "soc/soc_caps.h"
#include "esp_attr.h"
FL_EXTERN_C_END

// ============================================================================
// RMT DMA Platform Support Detection
// ============================================================================

/// @brief Compile-time DMA support detection for RMT peripheral
///
/// Platform DMA Support Matrix (based on SOC_RMT_SUPPORT_DMA):
/// - ESP32:     No DMA support
/// - ESP32-S2:  No DMA support
/// - ESP32-S3:  **YES** - DMA support available (1 channel only)
/// - ESP32-C3:  No DMA support
/// - ESP32-C6:  No DMA support
/// - ESP32-H2:  No DMA support
///
/// CRITICAL DMA ALLOCATION POLICY (ESP32-S3):
/// ==========================================
/// On ESP32-S3, the RMT peripheral has ONLY ONE DMA channel available.
/// This means:
/// - **FIRST channel created**: Uses DMA (if data size warrants it)
/// - **ALL subsequent channels**: MUST use non-DMA (on-chip memory)
///
/// The hardware limitation is enforced by tracking `mDMAChannelsInUse`:
/// - When mDMAChannelsInUse == 0: First channel can attempt DMA
/// - When mDMAChannelsInUse >= 1: All channels must use non-DMA
///
/// This is NOT a software limitation - it's a hardware constraint of the
/// ESP32-S3 RMT peripheral. Attempting to create multiple DMA channels
/// will fail at the ESP-IDF driver level.
///
/// Usage:
/// @code
/// #if FASTLED_RMT5_DMA_SUPPORTED
///     // Platform supports RMT DMA (currently only ESP32-S3)
///     if (mDMAChannelsInUse == 0) {
///         // First channel - can use DMA
///     } else {
///         // Subsequent channels - must use non-DMA
///     }
/// #else
///     // Platform does not support RMT DMA
/// #endif
/// @endcode
#ifdef SOC_RMT_SUPPORT_DMA
    #define FASTLED_RMT5_DMA_SUPPORTED 1
    #define FASTLED_RMT5_MAX_DMA_CHANNELS 1  ///< ESP32-S3 hardware limit: only 1 DMA channel
#else
    #define FASTLED_RMT5_DMA_SUPPORTED 0
    #define FASTLED_RMT5_MAX_DMA_CHANNELS 0  ///< No DMA support on this platform
#endif


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
// NOTE: 40MHz provides sufficient timing precision for modern LED protocols like WS2812B-V5
//       which require fine timing resolution (e.g., 645ns = 25.8 ticks at 40MHz vs 6.45 ticks at 10MHz)
#ifndef FASTLED_RMT5_CLOCK_HZ
#define FASTLED_RMT5_CLOCK_HZ 40000000  // 40MHz (25ns resolution) - matches RX frequency
#endif

// RMT memory configuration (matching RMT4)
#ifndef FASTLED_RMT_MEM_WORDS_PER_CHANNEL
#define FASTLED_RMT_MEM_WORDS_PER_CHANNEL SOC_RMT_MEM_WORDS_PER_CHANNEL
#endif

#ifndef FASTLED_RMT_MEM_BLOCKS
#define FASTLED_RMT_MEM_BLOCKS 2  // 2x as much memory for increased stability vs wifi jitter.
#endif

// Network-aware memory allocation
// When network is active (WiFi, Ethernet, or Bluetooth), use triple-buffering (3× memory blocks) for improved stability
// This helps compensate for increased interrupt latency caused by network operations
#ifndef FASTLED_RMT_MEM_BLOCKS_NETWORK_MODE
#define FASTLED_RMT_MEM_BLOCKS_NETWORK_MODE 3  // Triple-buffer for network activity
#endif

// Enable/disable Network-aware dynamic channel reduction
// When enabled, the RMT driver will automatically reduce the number of active
// channels when network is detected (WiFi, Ethernet, or Bluetooth), freeing up memory
// for the remaining channels to use triple-buffering. This prevents visual glitches during network activity.
#ifndef FASTLED_RMT_NETWORK_REDUCE_CHANNELS
#define FASTLED_RMT_NETWORK_REDUCE_CHANNELS 1  // Default: enabled
#endif

// Buffer size calculations
#define FASTLED_RMT5_MAX_PULSES (FASTLED_RMT_MEM_WORDS_PER_CHANNEL * FASTLED_RMT_MEM_BLOCKS)
#define FASTLED_RMT5_PULSES_PER_FILL (FASTLED_RMT5_MAX_PULSES / FASTLED_RMT_MEM_BLOCKS)  // Half buffer



// === Platform-Specific Signal Routing ===

// RMT signal routing for GPIO matrix
#if defined(CONFIG_IDF_TARGET_ESP32P4)
#define RMT_SIG_PAD_IDX RMT_SIG_PAD_OUT0_IDX
#else
#define RMT_SIG_PAD_IDX RMT_SIG_OUT0_IDX
#endif

// === Interrupt Priority Configuration ===
//
// RMT interrupts use level 3 on all platforms.
// Network interrupts (WiFi, Ethernet, Bluetooth) typically run at level 4, but the RMT5 driver uses ESP-IDF's
// interrupt handling (rmt_tx_register_event_callbacks), which only supports up
// to level 3 for C-based callbacks. Higher levels require assembly ISR handlers
// that are not exposed by the ESP-IDF RMT API.
//
// Level 3 is the maximum priority level supported by ESP-IDF's RMT driver.

// RMT interrupt priority (level 3 for all platforms)
#ifndef FL_RMT5_INTERRUPT_LEVEL
    #define FL_RMT5_INTERRUPT_LEVEL 3  // Use numeric value, not flag (ESP_INTR_FLAG_LEVEL3=8)
#endif

// Network-aware interrupt priority (same as normal - level 3)
// Currently no boost is possible without assembly ISR handlers
// This macro is kept for future expansion if needed
#ifndef FL_RMT5_INTERRUPT_LEVEL_NETWORK_MODE
    #define FL_RMT5_INTERRUPT_LEVEL_NETWORK_MODE 3  // Use numeric value, not flag
#endif

// Network priority boost feature flag
// Currently disabled on all platforms (both priorities are level 3)
// Kept for potential future use with alternative network interference mitigation strategies
#ifndef FASTLED_RMT_NETWORK_PRIORITY_BOOST
    #define FASTLED_RMT_NETWORK_PRIORITY_BOOST 0  // Disabled - no platforms support >level3
#endif


#endif // ESP32
