
// This is an prototype example for the ObjectFLED library for massive pins on teensy40/41.


#if !defined(__IMXRT1062__)   // Teensy 4.0/4.1 only.
void setup() {}
void loop() {}
#else

#include <Arduino.h>
#include "FastLED.h"

#include "third_party/object_fled/src/ObjectFLED.h"

// just one pin
#define NUM_CHANNELS 1
#define NUM_LEDS_PER_STRIP 1

byte pinList[NUM_CHANNELS] = {1};
CRGB leds[NUM_CHANNELS * NUM_LEDS_PER_STRIP];
ObjectFLED driver(NUM_CHANNELS * NUM_LEDS_PER_STRIP, leds, CORDER_RGB, NUM_CHANNELS, pinList);

void setup() {
    Serial.begin(9600);
    driver.begin(1.0, 250);  // zero overclock, 250uS LED latch delay (WS2812-V5B)
}



void blink(CRGB color, int times) {
    for (int i = 0; i < times; i++) {
        fill_solid(leds, NUM_CHANNELS * NUM_LEDS_PER_STRIP, color);
        driver.show();
        delay(500);
        fill_solid(leds, NUM_CHANNELS * NUM_LEDS_PER_STRIP, CRGB::Black);
        driver.show();
        delay(500);
    }
}

void loop() {
    Serial.println("Blinking...");
    blink(CRGB::Red, 1);
    blink(CRGB::Green, 2);
    blink(CRGB::Blue, 3);
}

#endif  // __IMXRT1062__
