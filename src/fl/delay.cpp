/// @file fl/delay.cpp
/// Runtime implementation for delay utilities

#include "fl/stdint.h"

#include "fl/delay.h"
#include "fl/types.h"

// ============================================================================
// Platform-provided delay functions (from Arduino or platform layer)
// ============================================================================
extern "C" {
void delay(unsigned long ms);
void delayMicroseconds(unsigned int us);
}

namespace fl {

// ============================================================================
// Millisecond and Microsecond delay implementations
// ============================================================================

/// Delay for a given number of milliseconds
/// Wraps the platform-provided delay() function
void delay(u32 ms) {
  ::delay((unsigned long)ms);
}

/// Delay for a given number of microseconds
/// Wraps the platform-provided delayMicroseconds() function
void delayMicroseconds(u32 us) {
  ::delayMicroseconds((unsigned int)us);
}

}  // namespace fl

#if defined(ESP32)
// ============================================================================
// ESP32 CPU frequency query (C interop)
// ============================================================================

extern "C" {
  uint32_t esp_clk_cpu_freq();
}

namespace fl {

/// Get the current ESP32 CPU frequency at runtime
/// Wraps the ESP-IDF C function esp_clk_cpu_freq()
uint32_t esp_clk_cpu_freq_impl() {
  return ::esp_clk_cpu_freq();
}

}  // namespace fl

#endif  // defined(ESP32)
