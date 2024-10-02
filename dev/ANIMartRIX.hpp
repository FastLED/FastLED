/*
  ___        _            ___  ______ _____    _      
 / _ \      (_)          / _ \ | ___ \_   _|  (_)     
/ /_\ \_ __  _ _ __ ___ / /_\ \| |_/ / | |_ __ ___  __
|  _  | '_ \| | '_ ` _ \|  _  ||    /  | | '__| \ \/ /
| | | | | | | | | | | | | | | || |\ \  | | |  | |>  < 
\_| |_/_| |_|_|_| |_| |_\_| |_/\_| \_| \_/_|  |_/_/\_\

by Stefan Petrick 2023.

High quality LED animations for your next project.

This is a Shader and 5D Coordinate Mapper made for realtime 
rendering of generative animations & artistic dynamic visuals.

This is also a modular animation synthesizer with waveform 
generators, oscillators, filters, modulators, noise generators, 
compressors... and much more.

VO.42 beta version
 
This code is licenced under a Creative Commons Attribution 
License CC BY-NC 3.0


*/

#warning "\
ANIMartRIX: Creative Commons Attribution License CC BY-NC-SA 4.0, free for non-commercial use only. \
For commercial use, please contact Stefan Petrick. Github: https://github.com/StefanPetrick/animartrix"

#include <FastLED.h>
#include "ANIMartRIX.h"

#define num_x  22                       // how many LEDs are in one row?
#define num_y  22                       // how many rows?

#define NUM_LED (num_x * num_y)
CRGB leds[NUM_LED];               // framebuffer

bool serpentine = true;

class FastLEDANIMartRIX : public ANIMartRIX {
  CRGB* leds;
  public:
  FastLEDANIMartRIX(int x, int y, CRGB* leds, bool serpentine) {
    this->init(x, y, serpentine);
    this->leds = leds;
  }
  void setPixelColor(int x, int y, rgb pixel) {
    leds[xy(x,y)] = CRGB(pixel.red, pixel.green, pixel.blue);
  }
  void setPixelColor(int index, rgb pixel) {
    leds[index] = CRGB(pixel.red, pixel.green, pixel.blue);
  }
};
FastLEDANIMartRIX animatrix(num_x, num_y, leds, serpentine);

//******************************************************************************************************************


void setup() {
  
  // FastLED.addLeds<NEOPIXEL, 13>(leds, NUM_LED);
  
  FastLED.addLeds<WS2811, 2, GRB>(leds, NUM_LED);   

  FastLED.setMaxPowerInVoltsAndMilliamps( 5, 2000); // optional current limiting [5V, 2000mA] 

  Serial.begin(115200);                 // check serial monitor for current fps count
 
 // fill_rainbow(leds, NUM_LED, 0);
  fill_solid(leds, NUM_LED, CRGB::Green);
  FastLED.show();
}

//*******************************************************************************************************************

void loop() {
  static uint8_t currentAnimation = 0;
  static const uint8_t numAnimations = 25;

  switch (currentAnimation) {
    case 0: animatrix.RGB_Blobs5(); break;
    case 1: animatrix.RGB_Blobs4(); break;
    case 2: animatrix.RGB_Blobs3(); break;
    case 3: animatrix.RGB_Blobs2(); break;
    case 4: animatrix.RGB_Blobs(); break;
    case 5: animatrix.Polar_Waves(); break;
    case 6: animatrix.Slow_Fade(); break;
    case 7: animatrix.Zoom2(); break;
    case 8: animatrix.Zoom(); break;
    case 9: animatrix.Hot_Blob(); break;
    case 10: animatrix.Spiralus2(); break;
    case 11: animatrix.Spiralus(); break;
    case 12: animatrix.Yves(); break;
    case 13: animatrix.Scaledemo1(); break;
    case 14: animatrix.Lava1(); break;
    case 15: animatrix.Caleido3(); break;
    case 16: animatrix.Caleido2(); break;
    case 17: animatrix.Caleido1(); break;
    case 18: animatrix.Distance_Experiment(); break;
    case 19: animatrix.Center_Field(); break;
    case 20: animatrix.Waves(); break;
    case 21: animatrix.Chasing_Spirals(); break;
    case 22: animatrix.Rotating_Blob(); break;
    case 23: animatrix.Rings(); break;
    default: animatrix.RGB_Blobs2(); break;
  }

  animatrix.logOutput();
  FastLED.show();
  animatrix.logFrame();
  EVERY_N_MILLIS(500) animatrix.report_performance();   // check serial monitor for report

  // Change animation every 10 seconds
  EVERY_N_SECONDS(10) {
    currentAnimation = (currentAnimation + 1) % numAnimations;
  }
}
