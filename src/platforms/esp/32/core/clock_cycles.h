
// ok no namespace fl
#pragma once

// IWYU pragma: private

#include "fl/stl/stdint.h"
#include "fl/stl/has_include.h"
#include "fl/stl/noexcept.h"

#include "fl/stl/stdint.h"
#include "platforms/esp/esp_version.h"
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
// IWYU pragma: begin_keep
#include "hal/cpu_hal.h"
// IWYU pragma: end_keep
#define __cpu_hal_get_cycle_count esp_cpu_get_cycle_count
#elif FL_HAS_INCLUDE(<hal/cpu_ll.h>)
  // esp-idf v4.3.0+
// IWYU pragma: begin_keep
#include <hal/cpu_ll.h>
// IWYU pragma: end_keep
inline fl::u32 __cpu_hal_get_cycle_count() FL_NO_EXCEPT {
  return static_cast<fl::u32>(cpu_ll_get_cycle_count());
}
#elif FL_HAS_INCLUDE(<esp_cpu.h>)  // First Fallback
// IWYU pragma: begin_keep
#include <esp_cpu.h>
// IWYU pragma: end_keep
inline fl::u32 __cpu_hal_get_cycle_count() FL_NO_EXCEPT {
  return static_cast<uint32>(esp_cpu_get_cycle_count());
}
#elif FL_HAS_INCLUDE(<xtensa/hal.h>)  // Second fallback
// IWYU pragma: begin_keep
#include <xtensa/hal.h>
// IWYU pragma: end_keep
inline fl::u32 __cpu_hal_get_cycle_count() FL_NO_EXCEPT {
  return static_cast<fl::u32>(xthal_get_ccount());
}
#else
// Last fallback: no IDF cycle-counter header was found. On Xtensa targets
// read the CCOUNT special register directly (matches the __clock_cycles()
// FASTLED_XTENSA path below). Non-Xtensa targets have no ROM/HAL fallback
// left to try, so fail the build loudly instead of silently depending on
// the Arduino core (<esp32-hal.h>) as earlier revisions of this file did.
inline fl::u32 __cpu_hal_get_cycle_count() FL_NO_EXCEPT {
#if defined(FASTLED_XTENSA) || defined(__XTENSA__)
  fl::u32 cyc;
  __asm__ __volatile__ ("rsr %0,ccount" : "=a" (cyc));
  return cyc;
#else
  #error "No ESP32 cycle-counter header found (hal/cpu_hal.h, hal/cpu_ll.h, esp_cpu.h, xtensa/hal.h). File a bug at github.com/FastLED/FastLED/issues with your board/toolchain."
#endif
}
#endif  // ESP_IDF_VERSION



__attribute__ ((always_inline)) inline static fl::u32 __clock_cycles() {
  fl::u32 cyc;
#ifdef FASTLED_XTENSA
  __asm__ __volatile__ ("rsr %0,ccount":"=a" (cyc)) FL_NO_EXCEPT;
#else
  cyc = __cpu_hal_get_cycle_count();
#endif
  return cyc;
}
