// BasicTest example to demonstrate how to use FastLED with OctoWS2811

// FastLED does not directly support Teensy 4.x PinList (for any
// number of pins) but it can be done with edits to FastLED code:
// https://www.blinkylights.blog/2021/02/03/using-teensy-4-1-with-fastled/

// Only compile this example on Teensy platforms with OctoWS2811 support
#if defined(TEENSYDUINO)
#include "OctoWS2811.h"
#else
#include <FastLED.h>

#define NUM_LEDS  1920

CRGB leds[NUM_LEDS];



#include "fake.h"
#endif // defined(__arm__) && defined(TEENSYDUINO)
