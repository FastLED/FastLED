/// @file platforms/esp/32/core/cpu_frequency.cpp.hpp
/// ESP32 CPU frequency runtime query implementation

#pragma once

#include "fl/int.h"

#if defined(ESP32) || defined(ARDUINO_ARCH_ESP32)

// ESP-IDF provides esp_clk_cpu_freq() as a C function
extern "C" {
  fl::u32 esp_clk_cpu_freq();
}

namespace fl {

/// Get the current ESP32 CPU frequency at runtime
/// Wraps the ESP-IDF C function esp_clk_cpu_freq()
/// @return CPU frequency in Hz
fl::u32 esp_clk_cpu_freq_impl() {
  return ::esp_clk_cpu_freq();
}

}  // namespace fl

#endif  // defined(ESP32) || defined(ARDUINO_ARCH_ESP32)
