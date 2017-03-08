#include <iostream>
#include <signal.h>
#include "FastLED.h"

FASTLED_USING_NAMESPACE

//
// This a version of the DemoReel100 for Linux.
//
// Before attempting to use this demo make sure the FastLED has been installed on your
// system.
//
// There's two ways to compile this demo:
//
// 1) Use the g++ command
//   g++ -std=gnu++11 -I/usr/local/include/FastLED LinuxDemo.cpp -o LinuxDemo -L/usr/local/lib -lfastled
// 2) Use the Makefile
//    make
//
// To run: ./LinuxDemo
//
// -- Michael Burg, 2016
//

// (Original comment header for DemoReel100)
// FastLED "100-lines-of-code" demo reel, showing just a few
// of the kinds of animation patterns you can quickly and easily
// compose using FastLED.
//
// This example also shows one easy way to define multiple
// animations patterns and have them automatically rotate.
//
// -Mark Kriegsman, December 2014

#if defined(FASTLED_VERSION) && (FASTLED_VERSION < 3001000)
#warning "Requires FastLED 3.1 or later; check github for latest code."
#endif

#define SPI_BUS    0
#define SPI_CS	   0

#define LED_TYPE    WS2801
#define COLOR_ORDER GRB
#define NUM_LEDS    64
CRGB leds[NUM_LEDS];

#define BRIGHTNESS          96
#define FRAMES_PER_SECOND  120

#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))

// List of patterns to cycle through.  Each is defined as a separate function below.
typedef void (*SimplePatternList[])();
uint8_t gCurrentPatternNumber = 0; // Index number of which pattern is current
uint8_t gHue = 0; // rotating "base color" used by many of the patterns

void workLoop();

void sigintHandler(int signum) {
  for (int i = 0; i < NUM_LEDS;i ++) {
    leds[i] = CRGB::Black;
  }

  FastLED.show();
  exit(0);
}

int main(int argc, char *argv[]) {
  try {
    FastLED.addLeds<LED_TYPE, SPI_BUS, SPI_CS, COLOR_ORDER>(leds, NUM_LEDS);
  } catch (std::exception const &exc) {
    std::cerr << "Exception caught " << exc.what() << "\n";
    exit(0);
  } catch (...) {
    std::cerr << "Unknown exception caught\n";
    exit(0);
  }

  signal(SIGINT, sigintHandler); // Handle when the user hits <CTRL>C to stop.

  // set master brightness control
  FastLED.setBrightness(BRIGHTNESS);

  for (;;) {
    workLoop();
  }
}


void rainbow()
{
  // FastLED's built-in rainbow generator
  fill_rainbow( leds, NUM_LEDS, gHue, 7);
}

void addGlitter( fract8 chanceOfGlitter)
{
  if( random8() < chanceOfGlitter) {
    leds[ random16(NUM_LEDS) ] += CRGB::White;
  }
}

void rainbowWithGlitter()
{
  // built-in FastLED rainbow, plus some random sparkly glitter
  rainbow();
  addGlitter(80);
}

void confetti()
{
  // random colored speckles that blink in and fade smoothly
  fadeToBlackBy( leds, NUM_LEDS, 10);
  int pos = random16(NUM_LEDS);
  leds[pos] += CHSV( gHue + random8(64), 200, 255);
}

void sinelon()
{
  // a colored dot sweeping back and forth, with fading trails
  fadeToBlackBy( leds, NUM_LEDS, 20);
  int pos = beatsin16(13,0,NUM_LEDS-1);
  leds[pos] += CHSV( gHue, 255, 192);
}

void bpm()
{
  // colored stripes pulsing at a defined Beats-Per-Minute (BPM)
  uint8_t BeatsPerMinute = 62;
  CRGBPalette16 palette = PartyColors_p;
  uint8_t beat = beatsin8( BeatsPerMinute, 64, 255);
  for( int i = 0; i < NUM_LEDS; i++) { //9948
    leds[i] = ColorFromPalette(palette, gHue+(i*2), beat-gHue+(i*10));
  }
}

void juggle() {
  // eight colored dots, weaving in and out of sync with each other
  fadeToBlackBy( leds, NUM_LEDS, 20);
  byte dothue = 0;
  for( int i = 0; i < 8; i++) {
    leds[beatsin16(i+7,0,NUM_LEDS-1)] |= CHSV(dothue, 200, 255);
    dothue += 32;
  }
}

SimplePatternList gPatterns = { rainbow, rainbowWithGlitter, confetti, sinelon, juggle, bpm };

void nextPattern()
{
  // add one to the current pattern number, and wrap around at the end
  gCurrentPatternNumber = (gCurrentPatternNumber + 1) % ARRAY_SIZE( gPatterns);
}

void workLoop() {
  // Call the current pattern function once, updating the 'leds' array
  gPatterns[gCurrentPatternNumber]();

  // FastLED.delay will call FastLED.show(), and then delay any
  // remaining milliseconds
  FastLED.delay(1000/FRAMES_PER_SECOND);

  // do some periodic updates
  EVERY_N_MILLISECONDS( 20 ) { gHue++; } // slowly cycle the "base color" through the rainbow
  EVERY_N_SECONDS( 10 ) { nextPattern(); } // change patterns periodically
}
