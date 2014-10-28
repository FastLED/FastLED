#include<FastLED.h>

#define NUM_LEDS_PER_STRIP 64
#define NUM_STRIPS 8

CRGB leds[NUM_STRIPS * NUM_LEDS_PER_STRIP];

// Pin layouts on the teensy 3:
// WS2811_PORTD: 2,14,7,8,6,20,21,5
// WS2811_PORTC: 15,22,23,9,10,13,11,12
//
// Pin layouts on the due
// WS2811_PORTA: 69,68,61,60,59,100,58,31 (note: pin 100 only available on the digix)
// WS2811_PORTB: 90,91,92,93,94,95,96,97 (note: only available on the digix)
// WS2811_PORTD: 25,26,27,28,14,15,29,11
//

void setup() {
// LEDS.addLeds<WS2811_PORTA,NUM_STRIPS>(leds, NUM_LEDS_PER_STRIP);
// LEDS.addLeds<WS2811_PORTB,NUM_STRIPS>(leds, NUM_LEDS_PER_STRIP);
// LEDS.addLeds<WS2811_PORTC,NUM_STRIPS>(leds, NUM_LEDS_PER_STRIP);
  LEDS.addLeds<WS2811_PORTD,NUM_STRIPS>(leds, NUM_LEDS_PER_STRIP);

  FastPin<0>::setOutput();
  LEDS.setBrightness(32);
}

void loop() {
  static uint8_t hue = 0;
  for(int i = 0; i < NUM_STRIPS; i++) {
    for(int j = 0; j < NUM_LEDS_PER_STRIP; j++) {
      leds[(i*NUM_LEDS_PER_STRIP) + j] = CHSV((32*i) + hue+j,192,255);
    }
  }

  // Set the first n leds on each strip to show which strip it is
  for(int i = 0; i < NUM_STRIPS; i++) {
    for(int j = 0; j <= i; j++) {
      leds[(i*NUM_LEDS_PER_STRIP) + j] = CRGB::Red;
    }
  }

  hue++;

  LEDS.show();
  LEDS.delay(10);
}
