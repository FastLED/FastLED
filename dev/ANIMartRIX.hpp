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
  //FastLED.clear();
  // art.RGB_Blobs5();
  // art.RGB_Blobs4();
  // art.RGB_Blobs3();
  animatrix.RGB_Blobs2();
  // animatrix.RGB_Blobs();
  // art.Polar_Waves();
  // art.Slow_Fade();
  // art.Zoom2();
  // art.Zoom();
  // art.Hot_Blob();
  // art.Spiralus2();
  // art.Spiralus();
  // art.Yves();
  // art.Scaledemo1();
  // art. art.Lava1();
  // art.Caleido3();
  // art.Caleido2();
  // art.Caleido1();
  // art.Distance_Experiment();
  // art.Center_Field();
  // art.Waves();
  // art.Chasing_Spirals();
  // art.Rotating_Blob();
  // art.Rings();
  animatrix.logOutput();
  FastLED.show();
  animatrix.logFrame();
  EVERY_N_MILLIS(500) animatrix.report_performance();   // check serial monitor for report 
} 
