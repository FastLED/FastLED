/// @file fl/stl/isr/constants.h
/// @brief ISR priority and flag constants

#pragma once

#include "fl/stl/stdint.h"

namespace fl {
namespace isr {

// Abstract priority levels (platform-independent, higher value = higher priority)
// Platforms map these to their native priority schemes internally
constexpr u8 ISR_PRIORITY_LOW        = 1;  // Lowest priority
constexpr u8 ISR_PRIORITY_DEFAULT    = 1;  // Default priority (alias for LOW)
constexpr u8 ISR_PRIORITY_MEDIUM     = 2;  // Medium priority
constexpr u8 ISR_PRIORITY_HIGH       = 3;  // High priority (recommended max)
constexpr u8 ISR_PRIORITY_CRITICAL   = 4;  // Critical (experimental, platform-dependent)
constexpr u8 ISR_PRIORITY_MAX        = 7;  // Maximum (may require assembly, platform-dependent)

// Common flags (applicable to most platforms)
constexpr u32 ISR_FLAG_IRAM_SAFE     = (1u << 0);  // Place in IRAM (ESP32)
constexpr u32 ISR_FLAG_EDGE_RISING   = (1u << 1);  // Edge-triggered, rising edge
constexpr u32 ISR_FLAG_EDGE_FALLING  = (1u << 2);  // Edge-triggered, falling edge
constexpr u32 ISR_FLAG_LEVEL_HIGH    = (1u << 3);  // Level-triggered, high level
constexpr u32 ISR_FLAG_LEVEL_LOW     = (1u << 4);  // Level-triggered, low level
constexpr u32 ISR_FLAG_ONE_SHOT      = (1u << 5);  // One-shot timer (vs auto-reload)
constexpr u32 ISR_FLAG_MANUAL_TICK   = (1u << 6);  // Manual tick mode (simulation/testing)

// ESP32-specific flags
constexpr u32 ISR_FLAG_ESP_SHARED    = (1ul << 16); // Shared interrupt (ESP32)
constexpr u32 ISR_FLAG_ESP_LOWMED    = (1ul << 17); // Low/medium priority shared (ESP32)

// STM32-specific flags
constexpr u32 ISR_FLAG_STM32_PREEMPT = (1ul << 18); // Use preemption priority (STM32)

} // namespace isr
} // namespace fl
