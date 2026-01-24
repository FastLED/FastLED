/*

// ok no namespace fl
  FastLED — Parallel Soft-SPI Host Simulation Layer
  --------------------------------------------------
  Ring buffer implementation for capturing GPIO events during host-side testing.
  Allows unit tests to verify ISR behavior without hardware.

  License: MIT (FastLED)
*/

#include "spi_platform.h"
#include "host_sim.h"

// Compile for host simulation OR stub platform (includes tests)
// STUB_PLATFORM now always gets real ISR implementations instead of stubs
#if defined(FASTLED_SPI_HOST_SIMULATION) || defined(STUB_PLATFORM)
#include "fl/stl/stdint.h"
#include "fl/stl/cstring.h"

/* Ring buffer for capturing GPIO events */
#define FL_GPIO_SIM_RING_SIZE 4096

typedef struct {
    FL_GPIO_Event events[FL_GPIO_SIM_RING_SIZE];
    uint32_t write_pos;
    uint32_t read_pos;
    uint32_t tick_counter;
    uint32_t overflow_count;
} FL_GPIO_RingBuffer;

static FL_GPIO_RingBuffer g_gpio_ring;

extern "C" {

/* Initialize ring buffer */
void fl_gpio_sim_init(void) {
    fl::memset(&g_gpio_ring, 0, sizeof(g_gpio_ring));
}

/* Capture SET event */
void fl_gpio_sim_write_set(uint32_t mask) {
    uint32_t pos = g_gpio_ring.write_pos;
    g_gpio_ring.events[pos].event_type = 0;  /* SET */
    g_gpio_ring.events[pos].gpio_mask = mask;
    g_gpio_ring.events[pos].timestamp = g_gpio_ring.tick_counter;

    g_gpio_ring.write_pos = (pos + 1) % FL_GPIO_SIM_RING_SIZE;
    if (g_gpio_ring.write_pos == g_gpio_ring.read_pos) {
        g_gpio_ring.overflow_count++;
    }
}

/* Capture CLEAR event */
void fl_gpio_sim_write_clear(uint32_t mask) {
    uint32_t pos = g_gpio_ring.write_pos;
    g_gpio_ring.events[pos].event_type = 1;  /* CLEAR */
    g_gpio_ring.events[pos].gpio_mask = mask;
    g_gpio_ring.events[pos].timestamp = g_gpio_ring.tick_counter;

    g_gpio_ring.write_pos = (pos + 1) % FL_GPIO_SIM_RING_SIZE;
    if (g_gpio_ring.write_pos == g_gpio_ring.read_pos) {
        g_gpio_ring.overflow_count++;
    }
}

/* Advance simulation time (called by test harness) */
void fl_gpio_sim_tick(void) {
    g_gpio_ring.tick_counter++;
}

/* Read event from ring buffer (returns false if empty) */
bool fl_gpio_sim_read_event(FL_GPIO_Event* out) {
    if (g_gpio_ring.read_pos == g_gpio_ring.write_pos) {
        return false;  /* Empty */
    }

    *out = g_gpio_ring.events[g_gpio_ring.read_pos];
    g_gpio_ring.read_pos = (g_gpio_ring.read_pos + 1) % FL_GPIO_SIM_RING_SIZE;
    return true;
}

/* Get event count */
uint32_t fl_gpio_sim_get_event_count(void) {
    if (g_gpio_ring.write_pos >= g_gpio_ring.read_pos) {
        return g_gpio_ring.write_pos - g_gpio_ring.read_pos;
    } else {
        return FL_GPIO_SIM_RING_SIZE - g_gpio_ring.read_pos + g_gpio_ring.write_pos;
    }
}

/* Clear ring buffer */
void fl_gpio_sim_clear(void) {
    g_gpio_ring.write_pos = 0;
    g_gpio_ring.read_pos = 0;
    g_gpio_ring.overflow_count = 0;
}

/* Get overflow count (for diagnostics) */
uint32_t fl_gpio_sim_get_overflow_count(void) {
    return g_gpio_ring.overflow_count;
}

}  // extern "C"

#endif /* FASTLED_SPI_HOST_SIMULATION */
