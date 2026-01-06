// @filter: (mem is high)
#include "FastLED.h"
#include "fl/fx/1d/twinklefox.h"

#define NUM_LEDS      100
#define LED_TYPE   WS2811
#define COLOR_ORDER   GRB
#define DATA_PIN        3
#define VOLTS          12
#define MAX_MA       4000

CRGBArray<NUM_LEDS> leds;
fl::TwinkleFox twinkleFox(NUM_LEDS);

void setup() {
  ::delay(3000UL); // safety startup delay
  FastLED.setMaxPowerInVoltsAndMilliamps(VOLTS, MAX_MA);
  FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS)
    .setCorrection(TypicalLEDStrip)
    .setRgbw();
}

void loop() {
  EVERY_N_SECONDS(SECONDS_PER_PALETTE) {
    twinkleFox.chooseNextColorPalette(twinkleFox.targetPalette);
  }
  twinkleFox.draw(fl::Fx::DrawContext(millis(), leds));
  FastLED.show();
}
