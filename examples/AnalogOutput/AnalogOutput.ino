// @filter: (board is not atmega8*)
/// @file    AnalogOutput.ino
/// @brief   Demonstrates how to use FastLED color functions even without a "pixel-addressible" smart LED strip.
/// @example AnalogOutput.ino

#include <Arduino.h>
#include <FastLED.h>



// Example showing how to use FastLED color functions
// even when you're NOT using a "pixel-addressible" smart LED strip.
//
// This example is designed to control an "analog" RGB LED strip
// (or a single RGB LED) being driven by Arduino PWM output pins.
// FastLED's ANALOG_RGB controller applies brightness, color correction, and
// color temperature before writing PWM duty cycles to the three output pins.
// 
// In this example, the RGB values are output on three separate
// 'analog' PWM pins, one for red, one for green, and one for blue.
 
#define REDPIN   5
#define GREENPIN 6
#define BLUEPIN  3

CRGB leds[1];

// Set the single analog RGB output color and update its PWM pins.
void showAnalogRGB( const CRGB& rgb)
{
  leds[0] = rgb;
  FastLED.show();
}



// colorBars: flashes Red, then Green, then Blue, then Black.
// Helpful for diagnosing if you've mis-wired which is which.
void colorBars()
{
  showAnalogRGB( CRGB::Red );   delay(500);
  showAnalogRGB( CRGB::Green ); delay(500);
  showAnalogRGB( CRGB::Blue );  delay(500);
  showAnalogRGB( CRGB::Black ); delay(500);
}

void loop() 
{
  static uint8_t hue;
  hue = hue + 1;
  // Use FastLED automatic HSV->RGB conversion
  showAnalogRGB( CHSV( hue, 255, 255) );
  
  delay(20);
}


void setup() {
  FastLED.addLeds<ANALOG_RGB, REDPIN, GREENPIN, BLUEPIN>(leds, 1)
    .setCorrection(TypicalLEDStrip)
    .setTemperature(DirectSunlight);
  FastLED.setBrightness(192);

  // Flash the "hello" color sequence: R, G, B, black.
  colorBars();
}
