#include "FastLED.h"
#include "fx/twinklefox.hpp"

#define NUM_LEDS      100
#define LED_TYPE   WS2811
#define COLOR_ORDER   GRB
#define DATA_PIN        2
#define VOLTS          12
#define MAX_MA       4000

CRGBArray<NUM_LEDS> leds;
TwinkleFoxData twinkleFoxData(leds, NUM_LEDS);

void setup() {
  delay(3000); // safety startup delay
  FastLED.setMaxPowerInVoltsAndMilliamps(VOLTS, MAX_MA);
  FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS)
    .setCorrection(TypicalLEDStrip)
    .setRgbw();

  chooseNextColorPalette(twinkleFoxData.targetPalette);
}

void loop() {
  EVERY_N_SECONDS(SECONDS_PER_PALETTE) { 
    chooseNextColorPalette(twinkleFoxData.targetPalette); 
  }
  
  EVERY_N_MILLISECONDS(10) {
    nblendPaletteTowardPalette(twinkleFoxData.currentPalette, twinkleFoxData.targetPalette, 12);
  }

  TwinkleFoxLoop(twinkleFoxData);
  
  FastLED.show();
}
