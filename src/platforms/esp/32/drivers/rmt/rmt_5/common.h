#pragma once

// ok no namespace fl

#include "platforms/esp/32/feature_flags/enabled.h"

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
#define FASTLED_RMT5_CLOCK_HZ 10000000  // 10MHz (100ns resolution)
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
