#include <FastLED.h>
#include "fx/1d/pride2015.hpp"

#define DATA_PIN    3
#define LED_TYPE    WS2811
#define COLOR_ORDER GRB
#define NUM_LEDS    200
#define BRIGHTNESS  255

CRGB leds[NUM_LEDS];
Pride2015Ref pride = Pride2015Ref::New(NUM_LEDS);

void setup() {
  delay(3000); // 3 second delay for recovery
  
  // tell FastLED about the LED strip configuration
  FastLED.addLeds<LED_TYPE,DATA_PIN,COLOR_ORDER>(leds, NUM_LEDS)
    .setCorrection(TypicalLEDStrip)
    .setDither(BRIGHTNESS < 255);

  // set master brightness control
  FastLED.setBrightness(BRIGHTNESS);
}

void loop() {
  pride->draw(Fx::DrawContext(millis(), leds));
  FastLED.show();  
}
