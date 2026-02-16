// ok no namespace fl
#pragma once

// IWYU pragma: private

#include "fl/compiler_control.h"
#include "is_avr.h"
// Defines a timer_millis for led_sysdefs_avr.h

// Please don't use this code, it's mostly to make certain platforms compile.
// If you use this code, then don't blame us if your code one day doesn't work.
// You should definatly define your own timer_millis, and make sure it updates correctly.
// The reason this code is here is because led_sysdefs_avr.h needs to bind to a timer source
// like timer_millis, or timer0_millis etc...
// A timer source is not critical to the AVR platform, because the Clockless drivers
// will work without it. The timer source is only used for the millis() function for these
// platforms.
#warning "Untested code: defining our own millis() timer, which is probably wrong."

#if defined(CLOCK_SOURCE) && (CLOCK_SOURCE == 0)
#warning "CLOCK_SOURCE=0 that the interrupt timer may not even work."
#endif

#if defined(FL_IS_AVR_ATTINY_MODERN) || defined(ARDUINO_attinyxy6)
#define DEFINE_AVR_TIMER_SOURCE_USES_TIMER0
#endif

#ifdef DEFINE_AVR_TIMER_SOURCE_USES_TIMER0

// IWYU pragma: begin_keep
#include <avr/io.h>
#include <avr/interrupt.h>
// IWYU pragma: end_keep

#ifndef F_CPU
#warning "F_CPU not defined for millis timer"
#define F_CPU 16000000UL
#endif

#define ABS(x) ((x) < 0 ? -(x) : (x))

FL_EXTERN_C_BEGIN
volatile unsigned long timer_millis = 0;
FL_EXTERN_C_END

ISR(TCA0_OVF_vect)
{
    // Increment timer_millis by 1 millisecond
    timer_millis++;
    // Clear the interrupt flag by writing a 1 to it
    TCA0.SINGLE.INTFLAGS = TCA_SINGLE_OVF_bm;
}

static void init()
{
    // Prescaler options for TCA0
    fl::u16 prescaler_values[] = {1, 2, 4, 8, 16, 64, 256, 1024};
    fl::u8 prescaler_gc_values[] = {
        TCA_SINGLE_CLKSEL_DIV1_gc,
        TCA_SINGLE_CLKSEL_DIV2_gc,
        TCA_SINGLE_CLKSEL_DIV4_gc,
        TCA_SINGLE_CLKSEL_DIV8_gc,
        TCA_SINGLE_CLKSEL_DIV16_gc,
        TCA_SINGLE_CLKSEL_DIV64_gc,
        TCA_SINGLE_CLKSEL_DIV256_gc,
        TCA_SINGLE_CLKSEL_DIV1024_gc
    };
    fl::u8 num_prescalers = sizeof(prescaler_values) / sizeof(prescaler_values[0]);

    fl::u16 prescaler = 0;
    fl::u16 period_counts = 0;
    fl::u8 prescaler_index = 0;

    fl::u32 min_error = 0xFFFFFFFF;

    for (fl::u8 i = 0; i < num_prescalers; i++)
    {
        fl::u16 current_prescaler = prescaler_values[i];
        fl::u32 counts = (F_CPU / current_prescaler) / 1000;
        if (counts == 0 || counts > 65535)
            continue;

        fl::u32 actual_period = (counts * current_prescaler * 1000000UL) / F_CPU; // in microseconds
        fl::i32 error = (fl::i32)actual_period - 1000; // error in microseconds

        if (ABS(error) < min_error)
        {
            min_error = ABS(error);
            prescaler = current_prescaler;
            period_counts = counts - 1;
            prescaler_index = i;
        }
    }

    if (prescaler == 0)
    {
        // Could not find suitable prescaler
        // Use maximum period and prescaler
        prescaler = prescaler_values[num_prescalers - 1];
        period_counts = 0xFFFF;
        prescaler_index = num_prescalers - 1;
    }

    // Set the period register
    TCA0.SINGLE.PER = period_counts;

    // Set the prescaler and enable the timer
    TCA0.SINGLE.CTRLA = prescaler_gc_values[prescaler_index] | TCA_SINGLE_ENABLE_bm;

    // Enable overflow interrupt
    TCA0.SINGLE.INTCTRL = TCA_SINGLE_OVF_bm;
}

// Global constructor to call init()
__attribute__((constructor))
static void avr_millis_init()
{
    init();
}

#else
#error "No timer defined for millis"
#endif  // DEFINE_AVR_TIMER_SOURCE_USES_TIMER0
