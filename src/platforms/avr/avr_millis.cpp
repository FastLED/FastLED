
// Defines a timer_millis for led_sysdefs_avr.h

#ifdef DEFINE_AVR_MILLIS
#warning "Untested code: using timer 0 for millis on ATtiny 0/1 series"
#if defined(__AVR_ATtinyxy6__) || defined(ARDUINO_attinyxy6)

#define DEFINE_AVR_MILLIS_USES_TIMER0
#endif




#ifdef DEFINE_AVR_MILLIS_USES_TIMER0

#include <avr/io.h>
#include <avr/interrupt.h>

#define MICROSECONDS_PER_TIMER0_OVERFLOW (64 * 256)


volatile unsigned long timer_millis = 0;


ISR(TCA0_OVF_vect)
{
    // ISR code
    timer_millis += MICROSECONDS_PER_TIMER0_OVERFLOW / 1000;
    // Clear the interrupt flag
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
#error "No timer defined for millis"
#endif  // DEFINE_AVR_MILLIS_USES_TIMER0
#endif // DEFINE_AVR_MILLIS
