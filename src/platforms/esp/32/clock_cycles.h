
#pragma once

#include <stdint.h>
#include "hal/cpu_hal.h"

__attribute__ ((always_inline)) inline static uint32_t __clock_cycles() {
  uint32_t cyc;
#ifdef FASTLED_XTENSA
  __asm__ __volatile__ ("rsr %0,ccount":"=a" (cyc));
#else
  cyc = cpu_hal_get_cycle_count();
#endif
  return cyc;
}