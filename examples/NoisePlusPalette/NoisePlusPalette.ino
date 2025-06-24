#include <FastLED.h>  // Main FastLED library for controlling LEDs

#if !SKETCH_HAS_LOTS_OF_MEMORY
// Don't compile this for AVR microcontrollers (like Arduino Uno) because they typically 
// don't have enough memory to handle this complex animation.
// Instead, we provide empty setup/loop functions so the sketch will compile but do nothing.
void setup() {}
void loop() {}
#else  // For all other platforms with more memory (ESP32, Teensy, etc.)
#include "NoisePlusPalette.h"
#endif  // End of the non-AVR code section
