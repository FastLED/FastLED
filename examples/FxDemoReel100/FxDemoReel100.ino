/// @file    DemoReel100.ino
/// @brief   FastLED "100 lines of code" demo reel, showing off some effects
/// @example DemoReel100.ino

#include <FastLED.h>
#include "fx/1d/demoreel100.h"

using namespace fl;

#define DATA_PIN    3
//#define CLK_PIN   4
#define LED_TYPE    WS2811
#define COLOR_ORDER GRB
#define NUM_LEDS    64
CRGB leds[NUM_LEDS];

#define BRIGHTNESS          96
#define FRAMES_PER_SECOND  120
#define USES_RGBW 0

#if USES_RGBW
Rgbw rgbwMode = RgbwDefault();
#else
Rgbw rgbwMode = RgbwInvalid();  // No RGBW mode, just use RGB.
#endif

DemoReel100Ptr demoReel = DemoReel100Ptr::New(NUM_LEDS);

void setup() {
  delay(3000); // 3 second delay for recovery
  
  // tell FastLED about the LED strip configuration
  FastLED.addLeds<LED_TYPE,DATA_PIN,COLOR_ORDER>(leds, NUM_LEDS)
    .setCorrection(TypicalLEDStrip)
    .setRgbw(rgbwMode);

  // set master brightness control
  FastLED.setBrightness(BRIGHTNESS);
}

void loop()
{
    // Run the DemoReel100 draw function
    demoReel->draw(Fx::DrawContext(millis(), leds));

    // send the 'leds' array out to the actual LED strip
    FastLED.show();  
    // insert a delay to keep the framerate modest
    FastLED.delay(1000/FRAMES_PER_SECOND); 
}


