#pragma once

#include "fl/compiler_control.h"

// Defines a weak (or strong for some attiny x/y parts) symbol for timer_millis
// to allow linking with boards whose core does not provide this variable.
// led_sysdefs_avr.h declares this with C linkage, so ensure the definition
// here also uses C linkage to avoid name-mangling/linking issues.

#ifdef __cplusplus
extern "C" {
#endif

#if defined(MILLIS_USE_TIMERD0) && defined(ARDUINO_attinyxy6)
// For ATtiny1616 and similar boards that use TIMERD0, provide a strong symbol
volatile unsigned long timer_millis = 0;
#else
FL_LINK_WEAK volatile unsigned long timer_millis = 0;
#endif

#ifdef __cplusplus
} // extern "C"
#endif
