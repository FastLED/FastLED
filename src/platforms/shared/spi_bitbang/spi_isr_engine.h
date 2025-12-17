/*

// ok no namespace fl
  FastLED — Parallel Soft-SPI ISR C API (RISC-V / ESP32-C3/C2)
  -------------------------------------------------------------
  Type definitions and C API declarations for the parallel SPI ISR.
  Can be included from both C and C++ code.

  License: MIT (FastLED)
*/

#ifndef FL_PARALLEL_SPI_ISR_RV_H
#define FL_PARALLEL_SPI_ISR_RV_H

#include "fl/stl/stdint.h"
#include "fl/compiler_control.h"

FL_EXTERN_C_BEGIN

/* --- Status bits (publish-only: ISR writes; main reads) --------------------- */
enum {
    FASTLED_STATUS_BUSY = 1u,
    FASTLED_STATUS_DONE = 2u
};

/* --- Validation buffer for debugging ---------------------------------------- */
/* Define FL_SPI_ISR_VALIDATE to enable GPIO event capture to side buffer      */
#ifdef FL_SPI_ISR_VALIDATE

/* 64KB validation buffer = 8192 events (8 bytes per event, perfectly aligned) */
#define FL_SPI_ISR_VALIDATE_SIZE 8192

/* GPIO event types */
typedef enum {
    FASTLED_GPIO_EVENT_STATE_START = 0,   /* Transfer started */
    FASTLED_GPIO_EVENT_STATE_DONE  = 1,   /* Transfer completed */
    FASTLED_GPIO_EVENT_SET_BITS    = 2,   /* GPIO W1TS (write-one-to-set) */
    FASTLED_GPIO_EVENT_CLEAR_BITS  = 3,   /* GPIO W1TC (write-one-to-clear) */
    FASTLED_GPIO_EVENT_CLOCK_LOW   = 4,   /* Clock went low (phase 0) */
    FASTLED_GPIO_EVENT_CLOCK_HIGH  = 5    /* Clock went high (phase 1) */
} FastLED_GPIO_Event_Type;

/* GPIO event record (8 bytes, perfectly aligned) */
typedef struct {
    uint8_t  event_type;  /* FastLED_GPIO_Event_Type */
    uint8_t  padding[3];  /* Align to 4 bytes */
    union {
        uint32_t gpio_mask;      /* For SET_BITS, CLEAR_BITS, CLOCK_LOW, CLOCK_HIGH */
        uint32_t state_info;     /* For STATE_START, STATE_DONE */
    } payload;
} FastLED_GPIO_Event;

#endif /* FL_SPI_ISR_VALIDATE */

/* --- Pin mask entry for lookup table ---------------------------------------- */
typedef struct {
    uint32_t set_mask;
    uint32_t clear_mask;
} PinMaskEntry;

/* --- Main ISR state structure ----------------------------------------------- */
typedef struct {
    /* Payload prepared by main (read by ISR after visibility delay): */
    PinMaskEntry pin_lookup_table[256];  /* byte -> {set_mask, clear_mask}      */
    uint8_t      spi_data_bytes[256];    /* transmit buffer                      */
    uint32_t     clock_pin_mask;         /* GPIO bit for software clock          */
    uint16_t     total_bytes_to_send;    /* burst length                         */

    /* Progress (owned by ISR during run): */
    uint16_t     current_position;       /* 0..total_bytes_to_send               */

    /* Edge-triggered arming (main -> ISR): */
    uint32_t     doorbell_counter;       /* main increments to signal new work   */
    uint32_t     last_processed_counter; /* last counter value ISR has consumed  */

    /* Publish-only back to main (ISR -> main): */
    uint32_t     status_flags;           /* BUSY/DONE                            */

    /* Local ISR phase flip-flop: 0=data+CLK low, 1=CLK high */
    uint8_t      clock_phase;

#ifdef FL_SPI_ISR_VALIDATE
    /* Validation buffer: captures raw GPIO events */
    FastLED_GPIO_Event validation_events[FL_SPI_ISR_VALIDATE_SIZE];
    uint16_t           validation_event_count;  /* number of events captured */
#endif
} FastLED_SPI_ISR_State;

/* --- ISR state accessor ---------------------------------------------------- */
FastLED_SPI_ISR_State* fl_spi_state(void);

/* --- Payload configuration (main thread) ----------------------------------- */
void fl_spi_set_clock_mask(uint32_t mask);
void fl_spi_set_total_bytes(uint16_t n);
void fl_spi_set_data_byte(uint16_t i, uint8_t v);
void fl_spi_set_lut_entry(uint8_t v, uint32_t set_m, uint32_t clr_m);

/* --- Direct LUT array access (main thread) --------------------------------- */
PinMaskEntry* fl_spi_get_lut_array(void);  /* Returns mutable reference to 256-entry LUT */
uint8_t*      fl_spi_get_data_array(void); /* Returns mutable reference to 256-byte data buffer */

/* --- Timing and control ---------------------------------------------------- */
void fl_spi_visibility_delay_us(uint32_t approx_us);
void fl_spi_arm(void);

/* --- Status accessors (main thread) ---------------------------------------- */
uint32_t fl_spi_status_flags(void);
void     fl_spi_ack_done(void);
void     fl_spi_reset_state(void);

/* --- ISR function ---------------------------------------------------------- */
void fl_parallel_spi_isr(void);

/* --- Platform-specific ISR setup/teardown ---------------------------------- */
int  fl_spi_platform_isr_start(uint32_t timer_hz);
void fl_spi_platform_isr_stop(void);

#ifdef FL_SPI_ISR_VALIDATE
/* --- Validation buffer accessors ------------------------------------------- */
const FastLED_GPIO_Event* fl_spi_get_validation_events(void);
uint16_t fl_spi_get_validation_event_count(void);
#endif

FL_EXTERN_C_END

#endif /* FL_PARALLEL_SPI_ISR_RV_H */
