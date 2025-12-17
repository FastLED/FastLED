/*

// ok no namespace fl
  FastLED — Parallel Soft-SPI Platform Abstraction Layer
  -------------------------------------------------------
  Provides platform-specific GPIO write abstractions for the ISR engine.

  Two modes:
  1. ESP32 hardware - Direct MMIO writes to GPIO registers
  2. Host simulation - Captures GPIO events to ring buffer for testing

  License: MIT (FastLED)
*/

#ifndef FL_PARALLEL_SPI_PLATFORM_H
#define FL_PARALLEL_SPI_PLATFORM_H

#include "fl/stl/stdint.h"
#include "fl/compiler_control.h"

/* Auto-enable host simulation mode on stub platform */
#if (defined(STUB_PLATFORM) || defined(FASTLED_STUB_IMPL)) && !defined(FASTLED_SPI_HOST_SIMULATION)
#define FASTLED_SPI_HOST_SIMULATION 1
#endif

FL_EXTERN_C_BEGIN

/* Platform-specific GPIO write functions */
/* ESP32: Direct MMIO writes */
/* Host:  Ring buffer event capture */

#ifdef FASTLED_SPI_HOST_SIMULATION
    /* Host simulation - capture events to ring buffer */
    void fl_gpio_sim_write_set(uint32_t mask);
    void fl_gpio_sim_write_clear(uint32_t mask);

    #define FL_GPIO_WRITE_SET(mask)   fl_gpio_sim_write_set(mask)
    #define FL_GPIO_WRITE_CLEAR(mask) fl_gpio_sim_write_clear(mask)
#else
    /* ESP32 hardware - direct MMIO */
    #ifndef FASTLED_GPIO_W1TS_ADDR
    #define FASTLED_GPIO_W1TS_ADDR 0x60004008u  /* ESP32-C3/C2 */
    #endif
    #ifndef FASTLED_GPIO_W1TC_ADDR
    #define FASTLED_GPIO_W1TC_ADDR 0x6000400Cu
    #endif

    #define FL_GPIO_WRITE_SET(mask) \
        (*(volatile uint32_t*)(uintptr_t)FASTLED_GPIO_W1TS_ADDR = (mask))
    #define FL_GPIO_WRITE_CLEAR(mask) \
        (*(volatile uint32_t*)(uintptr_t)FASTLED_GPIO_W1TC_ADDR = (mask))
#endif

FL_EXTERN_C_END

#endif /* FL_PARALLEL_SPI_PLATFORM_H */
