/*

// ok no namespace fl
  FastLED — Parallel Soft-SPI Host Simulation API
  ------------------------------------------------
  Public API for host simulation ring buffer and timer.
  Used by unit tests to verify ISR behavior.

  License: MIT (FastLED)
*/

#ifndef FL_PARALLEL_SPI_HOST_SIM_H
#define FL_PARALLEL_SPI_HOST_SIM_H

/* Auto-enable host simulation mode on stub platform */
#if defined(STUB_PLATFORM) && !defined(FASTLED_SPI_HOST_SIMULATION)
#define FASTLED_SPI_HOST_SIMULATION 1
#endif

#ifdef FASTLED_SPI_HOST_SIMULATION

#include "fl/stl/stdint.h"
#include "fl/compiler_control.h"

FL_EXTERN_C_BEGIN

/* GPIO event structure (matches internal ring buffer format) */
typedef struct {
    uint8_t  event_type;  /* 0=SET, 1=CLEAR */
    uint32_t gpio_mask;
    uint32_t timestamp;   /* Relative tick count */
} FL_GPIO_Event;

/* Ring buffer management */
void fl_gpio_sim_init(void);
void fl_gpio_sim_clear(void);
void fl_gpio_sim_tick(void);
bool fl_gpio_sim_read_event(FL_GPIO_Event* out);
uint32_t fl_gpio_sim_get_event_count(void);
uint32_t fl_gpio_sim_get_overflow_count(void);

/* Timer simulation */
bool fl_spi_host_timer_is_running(void);
uint32_t fl_spi_host_timer_get_hz(void);

FL_EXTERN_C_END

#endif /* FASTLED_SPI_HOST_SIMULATION */

#endif /* FL_PARALLEL_SPI_HOST_SIM_H */
