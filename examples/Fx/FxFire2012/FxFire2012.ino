/// @file    FxFire2012.ino
/// @brief   Fire2012 effect with ScreenMap
/// @example FxFire2012.ino
///
/// This sketch is fully compatible with the FastLED web compiler. To use it do the following:
/// 1. Install Fastled: `pip install fastled`
/// 2. cd into this examples page.
/// 3. Run the FastLED web compiler at root: `fastled`
/// 4. When the compiler is done a web page will open.

/// @brief   Simple one-dimensional fire animation function
// Fire2012 by Mark Kriegsman, July 2012
// as part of "Five Elements" shown here: http://youtu.be/knWiGsmgycY
//// 
// This basic one-dimensional 'fire' simulation works roughly as follows:
// There's a underlying array of 'heat' cells, that model the temperature
// at each point along the line.  Every cycle through the simulation, 
// four steps are performed:
//  1) All cells cool down a little bit, losing heat to the air
//  2) The heat from each cell drifts 'up' and diffuses a little
//  3) Sometimes randomly new 'sparks' of heat are added at the bottom
//  4) The heat from each cell is rendered as a color into the leds array
//     The heat-to-color mapping uses a black-body radiation approximation.
//
// Temperature is in arbitrary units from 0 (cold black) to 255 (white hot).
//
// This simulation scales it self a bit depending on NUM_LEDS; it should look
// "OK" on anywhere from 20 to 100 LEDs without too much tweaking. 
//
// I recommend running this simulation at anywhere from 30-100 frames per second,
// meaning an interframe delay of about 10-35 milliseconds.
//
// Looks best on a high-density LED setup (60+ pixels/meter).
//
//
// There are two main parameters you can play with to control the look and
// feel of your fire: COOLING (used in step 1 above), and SPARKING (used
// in step 3 above).
//
// COOLING: How much does the air cool as it rises?
// Less cooling = taller flames.  More cooling = shorter flames.
// Default 50, suggested range 20-100 

// SPARKING: What chance (out of 255) is there that a new spark will be lit?
// Higher chance = more roaring fire.  Lower chance = more flickery fire.
// Default 120, suggested range 50-200.

#include <FastLED.h>
#include "fx/1d/fire2012.h"
#include "fl/screenmap.h"

using namespace fl;

#define LED_PIN     5
#define COLOR_ORDER GRB
#define CHIPSET     WS2811
#define NUM_LEDS    92

#define BRIGHTNESS  128
#define FRAMES_PER_SECOND 30
#define COOLING 55
#define SPARKING 120
#define REVERSE_DIRECTION false

CRGB leds[NUM_LEDS];
Fire2012Ptr fire = fl::make_shared<Fire2012>(NUM_LEDS, COOLING, SPARKING, REVERSE_DIRECTION);

void setup() {
  ScreenMap screenMap = ScreenMap::DefaultStrip(NUM_LEDS, 1.5, .4);
  FastLED.addLeds<CHIPSET, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS)
    .setCorrection(TypicalLEDStrip)
    .setScreenMap(screenMap)
    .setRgbw();
  FastLED.setBrightness(BRIGHTNESS);
}

void loop()
{
  fire->draw(Fx::DrawContext(millis(), leds)); // run simulation frame
  
  FastLED.show(millis()); // display this frame
  FastLED.delay(1000 / FRAMES_PER_SECOND);
}
