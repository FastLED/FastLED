

/*
This demo is best viewed using the FastLED compiler.

Windows/MacOS binaries: https://github.com/FastLED/FastLED/releases

Python

Install: pip install fastled
Run: fastled <this sketch directory>
This will compile and preview the sketch in the browser, and enable
all the UI elements you see below.

OVERVIEW:
This sketch demonstrates a 2D wave simulation with multiple layers and blending effects.
It creates ripple effects that propagate across the LED matrix, similar to water waves.
The demo includes two wave layers (upper and lower) with different colors and properties,
which are blended together to create complex visual effects.
*/

#include <Arduino.h>      // Core Arduino functionality
#include <FastLED.h>      // Main FastLED library for controlling LEDs
#include "fl/sketch_macros.h"

#if !SKETCH_HAS_LOTS_OF_MEMORY
// Platform does not have enough memory
void setup() {}
void loop() {}
#else

#include "wavefx.h"

using namespace fl;        // Use the FastLED namespace for convenience


void setup() {
    Serial.begin(115200);  // Initialize serial communication for debugging
    wavefx_setup();
}

void loop() {
    // The main program loop that runs continuously
    wavefx_loop();
}
#endif
