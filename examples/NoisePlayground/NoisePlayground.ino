#include <FastLED.h>

#define kMatrixWidth  8
#define kMatrixHeight 5

#define NUM_LEDS (kMatrixWidth * kMatrixHeight)
// Param for different pixel layouts
#define kMatrixSerpentineLayout  false

// led array
CRGB leds[kMatrixWidth * kMatrixHeight];

// x,y, & time values
uint32_t x,y,v_time,hue_time;

// Play with the values of the variables below and see what kinds of effects they
// have!  More octaves will make things slower.

// how many octaves to use for the brightness and hue functions
uint8_t octaves=3;
uint8_t hue_octaves=3;

// the 'distance' between points on the x and y axis
int xscale=3311;
int yscale=3311;

// the 'distance' between x/y points for the hue noise
int hue_scale=1;

// how fast we move through time & hue noise
int time_speed=5101;
int hue_speed=1;

// adjust these values to move along the x or y axis between frames
int x_speed=0;
int y_speed=0;

void loop() {
  // fill the led array 2/16-bit noise values
  fill_2dnoise16(LEDS.leds(), kMatrixWidth, kMatrixHeight, kMatrixSerpentineLayout,
                octaves,x,xscale,y,yscale,v_time,
                hue_octaves,x,hue_scale,y,hue_scale,hue_time, false);
 static byte beacon = 0;
 beacon++;
 leds[0] = CHSV( beacon, 255, 255);

  LEDS.show();
  LEDS.countFPS();

  // adjust the intra-frame time values
  x += x_speed;
  y += y_speed;
  v_time += time_speed;
  hue_time += hue_speed;
  // delay(50);
}


void setup() {
  // initialize the x/y and time values
  random16_set_seed(18934);
  random16_add_entropy(analogRead(3));

  Serial.begin(57600);
  Serial.println("resetting!");

  delay(3000);
  LEDS.addLeds<WS2811,6,GRB>(leds,NUM_LEDS);
  LEDS.setBrightness(96);

  // x = (uint32_t)((uint32_t)random16() << 16) | random16();
  // y = (uint32_t)((uint32_t)random16() << 16) | random16();
  // v_time = (uint32_t)((uint32_t)random16() << 16) | random16();
  x = 1;
  y = 1;
  v_time = 1;
  hue_time = (uint32_t)((uint32_t)random16() << 16) | random16();

}
