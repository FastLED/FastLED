
// ok no namespace fl
#pragma once

// IWYU pragma: private

#include "fl/stl/stdint.h"
#include "fl/has_include.h"

#include "fl/stl/stdint.h"
#include "platforms/esp/esp_version.h"
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
// IWYU pragma: begin_keep
#include "hal/cpu_hal.h"
// IWYU pragma: end_keep
#define __cpu_hal_get_cycle_count esp_cpu_get_cycle_count
#elif FL_HAS_INCLUDE(<esp32-hal.h>) && ESP_IDF_VERSION > ESP_IDF_VERSION_VAL(4, 1, 4)
// IWYU pragma: begin_keep
#include <esp32-hal.h>  // Relies on the Arduino core for ESP32
// IWYU pragma: end_keep
inline fl::u32 __cpu_hal_get_cycle_count() {
  return static_cast<fl::u32>(cpu_hal_get_cycle_count());
}
#elif FL_HAS_INCLUDE(<hal/cpu_ll.h>)
  // esp-idf v4.3.0+
// IWYU pragma: begin_keep
#include <hal/cpu_ll.h>
// IWYU pragma: end_keep
inline fl::u32 __cpu_hal_get_cycle_count() {
  return static_cast<fl::u32>(cpu_ll_get_cycle_count());
}
#elif FL_HAS_INCLUDE(<esp_cpu.h>)  // First Fallback
// IWYU pragma: begin_keep
#include <esp_cpu.h>
// IWYU pragma: end_keep
inline fl::u32 __cpu_hal_get_cycle_count() {
  return static_cast<uint32>(esp_cpu_get_cycle_count());
}
#elif FL_HAS_INCLUDE(<xtensa/hal.h>)  // Second fallback
// IWYU pragma: begin_keep
#include <xtensa/hal.h>
// IWYU pragma: end_keep
inline fl::u32 __cpu_hal_get_cycle_count() {
  return static_cast<fl::u32>(xthal_get_ccount());
}
#else // Last fallback, if this fails then please file a bug at github.com/fastled/FastLED/issues and let us know what board you are using.
// IWYU pragma: begin_keep
#include <esp32-hal.h>  // Relies on the Arduino core for ESP32
// IWYU pragma: end_keep
inline fl::u32 __cpu_hal_get_cycle_count() {
  return static_cast<fl::u32>(cpu_hal_get_cycle_count());
}
#endif  // ESP_IDF_VERSION



__attribute__ ((always_inline)) inline static fl::u32 __clock_cycles() {
  fl::u32 cyc;
#ifdef FASTLED_XTENSA
  __asm__ __volatile__ ("rsr %0,ccount":"=a" (cyc));
#else
  cyc = __cpu_hal_get_cycle_count();
#endif
  return cyc;
}
