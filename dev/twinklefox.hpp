#include "FastLED.h"
#include "fx/1d/twinklefox.hpp"

#define NUM_LEDS      100
#define LED_TYPE   WS2811
#define COLOR_ORDER   BRG
#define DATA_PIN        2
#define VOLTS          12
#define MAX_MA       4000

CRGBArray<NUM_LEDS> leds;
TwinkleFox twinkleFox(NUM_LEDS);

void setup() {
  delay(3000); // safety startup delay
  FastLED.setMaxPowerInVoltsAndMilliamps(VOLTS, MAX_MA);
  FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS)
    .setCorrection(TypicalLEDStrip)
    .setRgbw();

  twinkleFox.lazyInit();
}

void loop() {
  EVERY_N_SECONDS(SECONDS_PER_PALETTE) { 
    twinkleFox.chooseNextColorPalette(twinkleFox.targetPalette); 
  }
  twinkleFox.draw(millis(), leds);
  FastLED.show();
}
