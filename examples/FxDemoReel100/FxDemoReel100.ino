/// @file    FxDemoReel100.ino
/// @brief   DemoReel100 effects collection with ScreenMap
/// @example FxDemoReel100.ino
///
/// This sketch is fully compatible with the FastLED web compiler. To use it do the following:
/// 1. Install Fastled: `pip install fastled`
/// 2. cd into this examples page.
/// 3. Run the FastLED web compiler at root: `fastled`
/// 4. When the compiler is done a web page will open.

#include <FastLED.h>
#include "fx/1d/demoreel100.h"
#include "fl/screenmap.h"
#include "defs.h"  // for NUM_LEDS


#if !HAS_ENOUGH_MEMORY
void setup() {}
void loop() {}
#else


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

DemoReel100Ptr demoReel = fl::make_shared<DemoReel100>(NUM_LEDS);

void setup() {
  ScreenMap screenMap = ScreenMap::DefaultStrip(NUM_LEDS);
  
  // tell FastLED about the LED strip configuration
  FastLED.addLeds<LED_TYPE,DATA_PIN,COLOR_ORDER>(leds, NUM_LEDS, 2.0f)
    .setCorrection(TypicalLEDStrip)
    .setScreenMap(screenMap)
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


#endif // HAS_ENOUGH_MEMORY
