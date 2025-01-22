/// @file    DemoReel100.ino
/// @brief   FastLED "100 lines of code" demo reel, showing off some effects
/// @example DemoReel100.ino

#include <FastLED.h>
#include "fx/1d/demoreel100.hpp"

#define DATA_PIN    2
//#define CLK_PIN   4
#define LED_TYPE    WS2811
#define COLOR_ORDER BRG
#define NUM_LEDS    64
CRGB leds[NUM_LEDS];

#define BRIGHTNESS          96
#define FRAMES_PER_SECOND  120

DemoReel100Ref demoReel = DemoReel100Ref::New(NUM_LEDS);

void setup() {
  delay(3000); // 3 second delay for recovery
  
  // tell FastLED about the LED strip configuration
  FastLED.addLeds<LED_TYPE,DATA_PIN,COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip).setRgbw();
  //FastLED.addLeds<LED_TYPE,DATA_PIN,CLK_PIN,COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);

  // set master brightness control
  FastLED.setBrightness(BRIGHTNESS);

  // Initialize the DemoReel100 instance
  demoReel.lazyInit();
}


void loop()
{
    // Run the DemoReel100 draw function
    demoReel.draw(millis(), leds);

    // send the 'leds' array out to the actual LED strip
    FastLED.show();  
    // insert a delay to keep the framerate modest
    FastLED.delay(1000/FRAMES_PER_SECOND); 
}


