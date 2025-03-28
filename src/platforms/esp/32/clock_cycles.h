
#pragma once

#include <stdint.h>

#include <stdint.h>
#include "platforms/esp/esp_version.h"
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
#include "hal/cpu_hal.h"
#define __cpu_hal_get_cycle_count esp_cpu_get_cycle_count
#elif __has_include(<esp32-hal.h>) && ESP_IDF_VERSION > ESP_IDF_VERSION_VAL(3, 0, 0)
#include <esp32-hal.h>  // Relies on the Arduino core for ESP32
inline uint32_t __cpu_hal_get_cycle_count() {
  return static_cast<uint32_t>(cpu_hal_get_cycle_count());
}
#elif __has_include(<hal/cpu_ll.h>)
  // esp-idf v4.3.0+
#include <hal/cpu_ll.h>
inline uint32_t __cpu_hal_get_cycle_count() {
  return static_cast<uint32_t>(cpu_ll_get_cycle_count());
}
#elif __has_include(<esp_cpu.h>)  // First Fallback
#include <esp_cpu.h>
inline uint32_t __cpu_hal_get_cycle_count() {
  return static_cast<uint32>(esp_cpu_get_cycle_count());
}
#elif __has_include(<xtensa/hal.h>)  // Second fallback
#include <xtensa/hal.h>
inline uint32_t __cpu_hal_get_cycle_count() {
  return static_cast<uint32_t>(xthal_get_ccount());
}
#else // Last fallback, if this fails then please file a bug at github.com/fastled/FastLED/issues and let us know what board you are using.
#include <esp32-hal.h>  // Relies on the Arduino core for ESP32
inline uint32_t __cpu_hal_get_cycle_count() {
  return static_cast<uint32_t>(cpu_hal_get_cycle_count());
}
#endif  // ESP_IDF_VERSION



__attribute__ ((always_inline)) inline static uint32_t __clock_cycles() {
  uint32_t cyc;
#ifdef FASTLED_XTENSA
  __asm__ __volatile__ ("rsr %0,ccount":"=a" (cyc));
#else
  cyc = __cpu_hal_get_cycle_count();
#endif
  return cyc;
}