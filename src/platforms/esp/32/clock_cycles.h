
#pragma once

#include <stdint.h>
#include "platforms/esp/esp_version.h"

#if __has_include(<hal/cpu_hal.h>)
  // esp-idf v5.0.0+
#include <hal/cpu_hal.h>
#define __cpu_hal_get_cycle_count esp_cpu_get_cycle_count
#elif __has_include(<hal/cpu_ll.h>)
  // esp-idf v4.3.0+
#include <hal/cpu_ll.h>
inline uint32_t __cpu_hal_get_cycle_count() {
  return static_cast<uint32_t>(cpu_ll_get_cycle_count());
}
#define __cpu_hal_get_cycle_count cpu_hal_get_cycle_count
#else
#error "We don't know how to get the cycle counts on your esp-idf version. Please file a bug at https://github.com/fastled/fastled/issues"
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