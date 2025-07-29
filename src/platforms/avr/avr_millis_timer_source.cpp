// Some attiny boards are missing a timer implementation. We provide two options
// which is to just link in a weak symbol that will resolve the timer_millis
// missing feature, or else implement it ourselves.

#ifdef __AVR__

#ifdef FASTLED_DEFINE_AVR_MILLIS_TIMER0_IMPL
#include "avr_millis_timer0_impl_source.hpp"
#else

#ifndef FASTLED_DEFINE_TIMER_WEAK_SYMBOL
#if !defined(MILLIS_USE_TIMERA0) \
    || defined(MILLIS_USE_TIMERD0) \
    || defined(__AVR_ATtinyxy6__) || defined(ARDUINO_attinyxy6) \
    || defined(__AVR_ATtinyxy7__) || defined(ARDUINO_attinyxy7) \
    || defined(__AVR_ATtinyxy8__) || defined(ARDUINO_attinyxy8) \
    || defined(__AVR_ATtinyxy4__) || defined(ARDUINO_attinyxy4) \
    || defined(__AVR_ATtinyxy5__) || defined(ARDUINO_attinyxy5) \
    || defined(__AVR_ATtiny1604__) \
    || defined(__AVR_ATtiny1616__) \
    || (defined(ARDUINO_attinyxy6) && defined(MILLIS_USE_TIMERD0))
#define FASTLED_DEFINE_TIMER_WEAK_SYMBOL 1
#else
#define FASTLED_DEFINE_TIMER_WEAK_SYMBOL 0
#endif
#endif  // FASTLED_DEFINE_TIMER_WEAK_SYMBOL


#if FASTLED_DEFINE_TIMER_WEAK_SYMBOL
#include "avr_millis_timer_null_counter.hpp"
#endif  // FASTLED_DEFINE_TIMER_WEAK_SYMBOL
#endif  // FASTLED_DEFINE_AVR_MILLIS_TIMER0_IMPL

#endif  // __AVR__
