/*

// ok no namespace fl
  FastLED — Parallel Soft-SPI Stub Implementations
  -------------------------------------------------
  Provides no-op stub implementations for SPI ISR functions on STUB_PLATFORM.
  These stubs are included in libfastled.a to satisfy linker dependencies from
  SpiIsr wrapper classes, while the real implementations are compiled separately
  for dedicated SPI ISR tests.

  License: MIT (FastLED)
*/

#include "spi_platform.h"
#include "spi_isr_engine.h"
#include "host_sim.h"

// Only compile stubs for STUB_PLATFORM when NOT building dedicated SPI ISR tests
// Test builds with FASTLED_SPI_ISR_TEST_BUILD get real implementations instead
#if defined(STUB_PLATFORM) && !defined(FASTLED_SPI_ISR_TEST_BUILD)

#include "fl/stdint.h"

extern "C" {

/* Stub implementations - do nothing */

FastLED_SPI_ISR_State* fl_spi_state(void) {
    static FastLED_SPI_ISR_State stub_state = {};
    return &stub_state;
}

void fl_spi_visibility_delay_us(uint32_t) {}
void fl_spi_arm(void) {}
uint32_t fl_spi_status_flags(void) { return 0; }
void fl_spi_ack_done(void) {}
void fl_spi_set_clock_mask(uint32_t) {}
void fl_spi_set_total_bytes(uint16_t) {}
void fl_spi_set_data_byte(uint16_t, uint8_t) {}
void fl_spi_set_lut_entry(uint8_t, uint32_t, uint32_t) {}
void fl_spi_reset_state(void) {}

PinMaskEntry* fl_spi_get_lut_array(void) {
    static PinMaskEntry stub_lut[256] = {};
    return stub_lut;
}

uint8_t* fl_spi_get_data_array(void) {
    static uint8_t stub_data[256] = {};
    return stub_data;
}

void fl_parallel_spi_isr(void) {}

int fl_spi_platform_isr_start(uint32_t) { return 0; }
void fl_spi_platform_isr_stop(void) {}
bool fl_spi_host_timer_is_running(void) { return false; }
uint32_t fl_spi_host_timer_get_hz(void) { return 0; }

#ifdef FASTLED_SPI_HOST_SIMULATION
void fl_gpio_sim_init(void) {}
void fl_gpio_sim_clear(void) {}
void fl_gpio_sim_tick(void) {}
bool fl_gpio_sim_read_event(FL_GPIO_Event*) { return false; }
uint32_t fl_gpio_sim_get_event_count(void) { return 0; }
uint32_t fl_gpio_sim_get_overflow_count(void) { return 0; }

// GPIO event capture stubs (used by SpiBlock implementations)
void fl_gpio_sim_write_set(uint32_t) {}
void fl_gpio_sim_write_clear(uint32_t) {}
#endif

#ifdef FL_SPI_ISR_VALIDATE
const FastLED_GPIO_Event* fl_spi_get_validation_events(void) {
    static FastLED_GPIO_Event stub_events[1] = {};
    return stub_events;
}
uint16_t fl_spi_get_validation_event_count(void) { return 0; }
#endif

}  // extern "C"

#endif  // STUB_PLATFORM && !FASTLED_SPI_ISR_TEST_BUILD
