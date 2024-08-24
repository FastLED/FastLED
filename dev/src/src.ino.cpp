# 1 "C:\\Users\\niteris\\AppData\\Local\\Temp\\tmpc7rlzukx"
#include <Arduino.h>
# 1 "C:/Users/niteris/dev/FastLED/dev/src/src.ino"




#include <FastLED.h>


#define NUM_LEDS 1





#define DATA_PIN 3
#define CLOCK_PIN 13


CRGB leds[NUM_LEDS];
void setup();
void loop();
#line 20 "C:/Users/niteris/dev/FastLED/dev/src/src.ino"
void setup() {
    FastLED.addLeds<WS2812, DATA_PIN>(leds, NUM_LEDS);
}

void loop() {

  leds[0] = CRGB::Red;
  FastLED.show();
  delay(500);

  leds[0] = CRGB::Black;
  FastLED.show();
  delay(500);
}