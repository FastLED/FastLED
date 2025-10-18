/// @file fl/delay.cpp
/// Runtime implementation for delay utilities

#include "fl/stdint.h"

#include "fl/delay.h"
#include "fl/types.h"

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
