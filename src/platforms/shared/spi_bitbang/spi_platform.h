// IWYU pragma: private

/*

// ok no namespace fl
  FastLED — Parallel Soft-SPI Platform Abstraction Layer
  -------------------------------------------------------
  Provides platform-specific GPIO write abstractions for the ISR driver.

  Two modes:
  1. ESP32 hardware (or an explicit custom backend) - Direct MMIO writes to GPIO registers
  2. Host simulation - Captures GPIO events to ring buffer for testing

  License: MIT (FastLED)
*/

#ifndef FL_PARALLEL_SPI_PLATFORM_H
#define FL_PARALLEL_SPI_PLATFORM_H

#include "fl/stl/stdint.h"
#include "fl/stl/compiler_control.h"
#include "fl/stl/noexcept.h"
#include "platforms/is_platform.h"

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
    void fl_gpio_sim_write_set(fl::u32 mask) FL_NO_EXCEPT;
    void fl_gpio_sim_write_clear(fl::u32 mask) FL_NO_EXCEPT;

    #define FL_GPIO_WRITE_SET(mask)   fl_gpio_sim_write_set(mask)
    #define FL_GPIO_WRITE_CLEAR(mask) fl_gpio_sim_write_clear(mask)
    #ifndef FL_SPI_GPIO_AVAILABLE
    #define FL_SPI_GPIO_AVAILABLE 1
    #endif
#else
    /* Hardware - direct MMIO. An explicit custom backend must give both addresses.
       Otherwise the addresses come from the target's own SDK: they differ between
       ESP32 families, and no other platform has a known set of them. */
    #if defined(FASTLED_GPIO_W1TS_ADDR) != defined(FASTLED_GPIO_W1TC_ADDR)
    #error "Define both FASTLED_GPIO_W1TS_ADDR and FASTLED_GPIO_W1TC_ADDR, or neither"
    #elif !defined(FASTLED_GPIO_W1TS_ADDR)
    #if defined(FL_IS_ESP32)
    // IWYU pragma: begin_keep
    #include "soc/gpio_reg.h"
    // IWYU pragma: end_keep
    /* Pins 0-31 only: these are the first bank's registers on every ESP32 family. */
    #define FASTLED_GPIO_W1TS_ADDR GPIO_OUT_W1TS_REG
    #define FASTLED_GPIO_W1TC_ADDR GPIO_OUT_W1TC_REG
    #else
    /* This header is compiled on every platform, so the failure waits for a real
       use: SpiBlock*::transmit() does not compile when this is 0. */
    #define FL_SPI_GPIO_AVAILABLE 0
    #endif
    #endif

    #ifndef FL_SPI_GPIO_AVAILABLE
    #define FL_SPI_GPIO_AVAILABLE 1
    #endif

    #if FL_SPI_GPIO_AVAILABLE
    #define FL_GPIO_WRITE_SET(mask) \
        (*(volatile fl::u32*)(uintptr_t)FASTLED_GPIO_W1TS_ADDR = (mask))
    #define FL_GPIO_WRITE_CLEAR(mask) \
        (*(volatile fl::u32*)(uintptr_t)FASTLED_GPIO_W1TC_ADDR = (mask))
    #else
    #define FL_GPIO_WRITE_SET(mask) ((void)(mask))
    #define FL_GPIO_WRITE_CLEAR(mask) ((void)(mask))
    #endif
#endif

FL_EXTERN_C_END

#endif /* FL_PARALLEL_SPI_PLATFORM_H */
