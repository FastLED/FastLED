#pragma once

// Defines a weak symbol for timer_millis to allow linking with some Attiny boards
// This is a weak definition of timer_millis.
// It allows the code to compile and link, but it's expected that
// the actual implementation will be provided elsewhere (e.g., by the Arduino core).

#if defined(MILLIS_USE_TIMERD0) && defined(ARDUINO_attinyxy6)
// For ATtiny1616 and similar boards that use TIMERD0, we provide a strong timer_millis symbol
// Since these boards don't provide timer_millis but FastLED expects it, we create a simple counter
volatile unsigned long timer_millis = 0;
#else
__attribute__((weak)) volatile unsigned long timer_millis = 0;
#endif
