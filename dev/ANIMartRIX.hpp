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
ANIMartRIX: free for non-commercial use and licensed under the Creative Commons Attribution License CC BY-NC-SA 4.0, . \
For commercial use, please contact Stefan Petrick. Github: https://github.com/StefanPetrick/animartrix". \
Modified by github.com/netmindz for class portability. \
Modified by Zach Vorhies for FastLED fx compatibility."


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

enum Animation {
  RGB_BLOBS5,
  RGB_BLOBS4,
  RGB_BLOBS3,
  RGB_BLOBS2,
  RGB_BLOBS,
  POLAR_WAVES,
  SLOW_FADE,
  ZOOM2,
  ZOOM,
  HOT_BLOB,
  SPIRALUS2,
  SPIRALUS,
  YVES,
  SCALEDEMO1,
  LAVA1,
  CALEIDO3,
  CALEIDO2,
  CALEIDO1,
  DISTANCE_EXPERIMENT,
  CENTER_FIELD,
  WAVES,
  CHASING_SPIRALS,
  ROTATING_BLOB,
  RINGS,
  NUM_ANIMATIONS
};

void loop() {
  static Animation currentAnimation = RGB_BLOBS5;

  switch (currentAnimation) {
    case RGB_BLOBS5: animatrix.RGB_Blobs5(); break;
    case RGB_BLOBS4: animatrix.RGB_Blobs4(); break;
    case RGB_BLOBS3: animatrix.RGB_Blobs3(); break;
    case RGB_BLOBS2: animatrix.RGB_Blobs2(); break;
    case RGB_BLOBS: animatrix.RGB_Blobs(); break;
    case POLAR_WAVES: animatrix.Polar_Waves(); break;
    case SLOW_FADE: animatrix.Slow_Fade(); break;
    case ZOOM2: animatrix.Zoom2(); break;
    case ZOOM: animatrix.Zoom(); break;
    case HOT_BLOB: animatrix.Hot_Blob(); break;
    case SPIRALUS2: animatrix.Spiralus2(); break;
    case SPIRALUS: animatrix.Spiralus(); break;
    case YVES: animatrix.Yves(); break;
    case SCALEDEMO1: animatrix.Scaledemo1(); break;
    case LAVA1: animatrix.Lava1(); break;
    case CALEIDO3: animatrix.Caleido3(); break;
    case CALEIDO2: animatrix.Caleido2(); break;
    case CALEIDO1: animatrix.Caleido1(); break;
    case DISTANCE_EXPERIMENT: animatrix.Distance_Experiment(); break;
    case CENTER_FIELD: animatrix.Center_Field(); break;
    case WAVES: animatrix.Waves(); break;
    case CHASING_SPIRALS: animatrix.Chasing_Spirals(); break;
    case ROTATING_BLOB: animatrix.Rotating_Blob(); break;
    case RINGS: animatrix.Rings(); break;
  }

  animatrix.logOutput();
  FastLED.show();
  animatrix.logFrame();
  EVERY_N_MILLIS(500) animatrix.report_performance();   // check serial monitor for report

  // Change animation every 10 seconds
  EVERY_N_SECONDS(10) {
    currentAnimation = static_cast<Animation>((static_cast<int>(currentAnimation) + 1) % NUM_ANIMATIONS);
  }
}
