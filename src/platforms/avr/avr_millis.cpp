

// Defines a timer_millis for led_sysdefs_avr.h

#ifndef DEFINE_AVR_MILLIS
#if defined(__AVR_ATtinyxy6__)
#define DEFINE_AVR_MILLIS 1
#define DEFINE_AVR_MILLIS_USES_TIMER0
#else
#define DEFINE_AVR_MILLIS 0
#endif
#endif

#if DEFINE_AVR_MILLIS=1

#if DEFINE_AVR_MILLIS_USES_TIMER0

#include <avr/io.h>
#include <avr/interrupt.h>

#define MICROSECONDS_PER_TIMER0_OVERFLOW (64 * 256)


volatile unsigned long timer0_overflow_count = 0;
extern volatile unsigned long timer_millis;


ISR(TCA0_OVF_vect)
{
    timer_millis += MICROSECONDS_PER_TIMER0_OVERFLOW / 1000;
    timer0_overflow_count++;
    TCA0.SINGLE.INTFLAGS = TCA_SINGLE_OVF_bm;
}

static void init()
{
    // Set up Timer A for millis
    TCA0.SINGLE.CTRLA = TCA_SINGLE_CLKSEL_DIV64_gc | TCA_SINGLE_ENABLE_bm;
    TCA0.SINGLE.INTCTRL = TCA_SINGLE_OVF_bm;
}

// Global constructor to call init()
__attribute__((constructor))
static void avr_millis_init()
{
    init();
}

#else
#error "Timer not defined for this AVR"
#endif  // DEFINE_AVR_MILLIS_USES_TIMER0

#endif  // DEFINE_AVR_MILLIS

volatile unsigned long timer_millis = 0;
