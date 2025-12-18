/*

// ok no namespace fl
  FastLED — Parallel Soft-SPI ISR (RISC-V / ESP32-C3/C2 AHB GPIO)
  ----------------------------------------------------------------
  Goals:
    • ISR performs NO volatile reads; only volatile WRITES (GPIO + status).
    • Producer/consumer via a monotonic doorbell counter (edge-triggered).
    • Two-phase bit engine:
        Phase 0: data pins + CLK LOW  (W1TS data-high, W1TC data-low|CLKlow)
        Phase 1: CLK HIGH             (W1TS clock)
    • All ISR-visible fields are aggregated in a single struct for clarity.

  Integration notes:
    • This unit is self-contained. You can:
        (A) Build into your firmware as normal C++, or
        (B) Compile with -S to emit .S and inline the body at your top ISR vector.
    • For NMI/Level-7 use, place a *very small* assembly wrapper that jumps into
      `fl_parallel_spi_isr()` and returns with your platform's trap return.
    • Keep the ISR in IRAM and data in DRAM (or preloaded) for predictability.

  GPIO MMIO (ESP32-C3/C2 AHB):
    W1TS: 0x60004008  (write 1 to set pin bits)
    W1TC: 0x6000400C  (write 1 to clear pin bits)

  Build tips:
    riscv32-esp-elf-g++ -Os -ffreestanding -fno-asynchronous-unwind-tables \
      -fno-unwind-tables -fno-exceptions -fno-rtti \
      -S fl_parallel_spi_isr_rv.S.cpp -o fl_parallel_spi_isr_rv.S

  License: MIT (FastLED)
*/

#include "fl/sketch_macros.h"

// Include platform header first to enable auto-detection of FASTLED_SPI_HOST_SIMULATION
#include "spi_platform.h"

// Compile for platforms with sufficient memory OR stub platform (includes tests)
// STUB_PLATFORM now always gets real ISR implementations instead of stubs
#if SKETCH_HAS_LOTS_OF_MEMORY || defined(STUB_PLATFORM)

#include "fl/stl/stdint.h"
#include "fl/compiler_control.h"
#include "spi_isr_engine.h"
#include "fl/stl/atomic.h"


// All implementation uses C linkage for clean assembly generation
FL_EXTERN_C_BEGIN

/* Single instance (internal linkage). Provide accessors below for host code. */
static FastLED_SPI_ISR_State g_isr_state;

/* --- Host-side convenience (optional, small & header-free) ------------------ */
FastLED_SPI_ISR_State* fl_spi_state(void) { return &g_isr_state; }

/* Visibility: crude ~microsecond spin (portable). Replace with platform fence if desired. */
void fl_spi_visibility_delay_us(uint32_t approx_us) {
  volatile uint32_t spin = 0;
  /* Tune constant per CPU; here ~100 cycles/us at 240 MHz, coarse. */
  FL_DISABLE_WARNING_PUSH
  FL_DISABLE_WARNING_VOLATILE
  for (uint32_t i = 0; i < approx_us * 100u; ++i) spin += 1;
  FL_DISABLE_WARNING_POP
  (void)spin;
}

/* Arm: ring the doorbell AFTER payload & delay */
void fl_spi_arm(void) {
  g_isr_state.doorbell_counter++;
}

/* Status accessors (main thread) */
uint32_t fl_spi_status_flags(void) {
  return g_isr_state.status_flags;
}
void fl_spi_ack_done(void) {
  g_isr_state.status_flags &= ~FASTLED_STATUS_DONE;
}

/* Payload setters (main thread) */
void fl_spi_set_clock_mask(uint32_t mask) { g_isr_state.clock_pin_mask = mask; }
void fl_spi_set_total_bytes(uint16_t n)   { g_isr_state.total_bytes_to_send = n; }
void fl_spi_set_data_byte(uint16_t i, uint8_t v){ g_isr_state.spi_data_bytes[i] = v; }
void fl_spi_set_lut_entry(uint8_t v, uint32_t set_m, uint32_t clr_m) {
  g_isr_state.pin_lookup_table[v].set_mask  = set_m;
  g_isr_state.pin_lookup_table[v].clear_mask= clr_m;
}

/* Optional reset (safe between runs) */
void fl_spi_reset_state(void) {
  g_isr_state.current_position       = 0;
  g_isr_state.last_processed_counter = g_isr_state.doorbell_counter;
  g_isr_state.status_flags           = 0;
  g_isr_state.clock_phase            = 0;
#ifdef FL_SPI_ISR_VALIDATE
  g_isr_state.validation_event_count = 0;
#endif

}

/* Direct array accessors (main thread) */
PinMaskEntry* fl_spi_get_lut_array(void) {
  return g_isr_state.pin_lookup_table;
}

uint8_t* fl_spi_get_data_array(void) {
  return g_isr_state.spi_data_bytes;
}

#ifdef FL_SPI_ISR_VALIDATE
/* Log GPIO event to validation buffer */
static inline void fl_spi_log_event(FastLED_GPIO_Event_Type type, uint32_t payload) {
  if (g_isr_state.validation_event_count < FL_SPI_ISR_VALIDATE_SIZE) {
    FastLED_GPIO_Event *evt = &g_isr_state.validation_events[g_isr_state.validation_event_count++];
    evt->event_type = (uint8_t)type;
    evt->padding[0] = 0;
    evt->padding[1] = 0;
    evt->padding[2] = 0;
    evt->payload.gpio_mask = payload;
  }
}

/* Validation buffer accessors (main thread) */
const FastLED_GPIO_Event* fl_spi_get_validation_events(void) {
  return g_isr_state.validation_events;
}
uint16_t fl_spi_get_validation_event_count(void) {
  return g_isr_state.validation_event_count;
}
#endif

/* --- The ISR body (zero volatile reads) ------------------------------------ */
/* Place in IRAM (attribute depends on platform). For .S generation, just -S.  */
/* Note: Function has C linkage via extern "C" block above                      */
void fl_parallel_spi_isr(void) {
  /* 1) Edge detect: new work? */
  uint32_t current_doorbell = g_isr_state.doorbell_counter;
  if (current_doorbell != g_isr_state.last_processed_counter) {
    g_isr_state.last_processed_counter = current_doorbell;
    g_isr_state.current_position       = 0;
    g_isr_state.status_flags          |= FASTLED_STATUS_BUSY;
    g_isr_state.clock_phase            = 0; /* start with data+CLK low */
#ifdef FL_SPI_ISR_VALIDATE
    fl_spi_log_event(FASTLED_GPIO_EVENT_STATE_START, g_isr_state.total_bytes_to_send);
#endif
  }

  /* 2) Nothing to send? Mark as done and return (but only if we're not in phase 1). */
  if (g_isr_state.current_position >= g_isr_state.total_bytes_to_send) {
    /* If we're in phase 1, we need to complete it (raise clock high) before returning */
    if (g_isr_state.clock_phase == 0) {
      /* Phase 0: No more bytes to send, mark as done */
      if (g_isr_state.status_flags & FASTLED_STATUS_BUSY) {
        uint32_t s = g_isr_state.status_flags;
        s &= ~FASTLED_STATUS_BUSY;
        s |=  FASTLED_STATUS_DONE;
        g_isr_state.status_flags = s;
#ifdef FL_SPI_ISR_VALIDATE
        fl_spi_log_event(FASTLED_GPIO_EVENT_STATE_DONE, g_isr_state.current_position);
#endif
      }
      return;
    }
    /* Phase 1: Fall through to complete the clock high cycle */
  }

  /* 3) Two-phase engine */
  if (g_isr_state.clock_phase == 0) {
    /* Phase 0: present data + force CLK low */
    if (g_isr_state.current_position >= g_isr_state.total_bytes_to_send) {
      g_isr_state.clock_phase = 1; /* final edge will occur in Phase 1 */
      return;
    }

    uint8_t  next_data = g_isr_state.spi_data_bytes[g_isr_state.current_position++];
    uint32_t pins_to_set   = g_isr_state.pin_lookup_table[next_data].set_mask;
    uint32_t pins_to_clear = g_isr_state.pin_lookup_table[next_data].clear_mask | g_isr_state.clock_pin_mask;

#ifdef FL_SPI_ISR_VALIDATE
    /* Log GPIO operations */
    fl_spi_log_event(FASTLED_GPIO_EVENT_SET_BITS, pins_to_set);
    fl_spi_log_event(FASTLED_GPIO_EVENT_CLEAR_BITS, pins_to_clear);
    fl_spi_log_event(FASTLED_GPIO_EVENT_CLOCK_LOW, g_isr_state.clock_pin_mask);
#endif

    FL_GPIO_WRITE_SET(pins_to_set);      /* data-high bits */
    FL_GPIO_WRITE_CLEAR(pins_to_clear);  /* data-low bits + CLK low */

    g_isr_state.clock_phase = 1;
    return;

  } else {
    /* Phase 1: raise CLK high to latch data */
#ifdef FL_SPI_ISR_VALIDATE
    fl_spi_log_event(FASTLED_GPIO_EVENT_CLOCK_HIGH, g_isr_state.clock_pin_mask);
#endif

    FL_GPIO_WRITE_SET(g_isr_state.clock_pin_mask);

    /* If we've emitted the last byte, this rise completes the burst. */
    if (g_isr_state.current_position >= g_isr_state.total_bytes_to_send) {
      uint32_t s = g_isr_state.status_flags;
      s &= ~FASTLED_STATUS_BUSY;
      s |=  FASTLED_STATUS_DONE;
      g_isr_state.status_flags = s;
#ifdef FL_SPI_ISR_VALIDATE
      fl_spi_log_event(FASTLED_GPIO_EVENT_STATE_DONE, g_isr_state.current_position);
#endif
      /* Timer disable/ack (if any) belongs in your vector wrapper. */
    }

    g_isr_state.clock_phase = 0;
    return;
  }
}

FL_EXTERN_C_END


#endif  // SKETCH_HAS_LOTS_OF_MEMORY && (!defined(STUB_PLATFORM) || defined(FASTLED_SPI_HOST_SIMULATION))
