/*
  FastLED — Parallel Soft-SPI Host Timer Simulation
  --------------------------------------------------
  Timer simulation for host-side testing. Emulates ESP32 hardware timer ISR.
  Tests drive ISR manually via fl_spi_host_simulate_tick().

  Implementation Note - Side Thread Emulation:
  The software bitbanger ISR implementations are designed to mock out ESP32
  hardware tests on the host machine. Current implementation uses single-threaded
  mode where tests manually call fl_spi_host_simulate_tick() in a loop for
  deterministic timing and easy debugging.

  License: MIT (FastLED)
*/

#ifdef FASTLED_SPI_HOST_SIMULATION

#include "fl_parallel_spi_isr_rv.h"
#include "fl_parallel_spi_host_sim.h"
#include <stdint.h>

/* Host-side timer simulation - emulates ESP32 hardware timer ISR */
/* NOTE: Tests drive ISR manually via fl_spi_host_simulate_tick()   */
/* Future enhancement: Optional side thread for real-time emulation */

static bool g_timer_running = false;
static uint32_t g_timer_hz = 0;

extern "C" {

/* Start timer (initializes host simulation) */
int fl_spi_platform_isr_start(uint32_t timer_hz) {
    g_timer_hz = timer_hz;
    g_timer_running = true;
    fl_gpio_sim_init();
    return 0;  /* Success */
}

/* Stop timer */
void fl_spi_platform_isr_stop(void) {
    g_timer_running = false;
}

/* Test harness calls this to simulate timer tick (mock ESP32 timer ISR) */
void fl_spi_host_simulate_tick(void) {
    if (g_timer_running) {
        fl_parallel_spi_isr();  /* Call ISR (same code as ESP32) */
        fl_gpio_sim_tick();     /* Advance time in ring buffer */
    }
}

/* Query timer state */
bool fl_spi_host_timer_is_running(void) {
    return g_timer_running;
}

uint32_t fl_spi_host_timer_get_hz(void) {
    return g_timer_hz;
}

}  // extern "C"

/* Future: Optional side thread for real-time emulation */
/*
void* fl_spi_host_timer_thread(void* arg) {
    // Sleep for 1/timer_hz seconds
    // Call fl_spi_host_simulate_tick()
    // Repeat until g_timer_running is false
}
*/

#endif /* FASTLED_SPI_HOST_SIMULATION */
