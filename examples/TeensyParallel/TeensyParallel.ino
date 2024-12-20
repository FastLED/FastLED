
// This is an prototype example for the ObjectFLED library for massive pins on teensy40/41.


#if !defined(__IMXRT1062__)   // Teensy 4.0/4.1 only.
void setup() {}
void loop() {}
#else

#include "third_party/object_fled/src/ObjectFLED.h"
#include <FastLED.h>

//configure your test cube with display object = leds, draw buffer = testCube[Z][Y][X].
//configure a separate cube to pre-clear any physical LEDs you actually have connected
//  with display object = blanks, draw buffer = blankLeds[Z][Y][X].
//notice that with ObjectFLED you can define 2 display objects to drive the same physical 
//  object on the same physical pins.
const int PIX_PER_ROW = 16,
          NUM_ROWS = 16,
          NUM_PLANES = 16,
          NUM_CHANNELS = 16,
          COLOR_ORDER = RGB,
          //LED baud * 3 bits/LED byte = Serial baud; 2.4 MHz serial,  3.6 passed testing
          LED_SERIAL_BAUD = 800 * 1.8,  //1200,         //SerialFLED in kHz  
          STD_OUT_BAUD = 100000;
const CRGB background = 0x505000;
byte pinList[NUM_CHANNELS] = {1, 8, 14, 17, 24, 29, 20, 0, 15, 16, 18, 19, 21, 22, 23, 25};
byte pinListBlank[7] = {1, 8, 14, 17, 24, 29, 20};
const int config = CORDER_RGB;

CRGB testCube[NUM_PLANES][NUM_ROWS][PIX_PER_ROW];
CRGB blankLeds[7][8][8];
ObjectFLED leds(PIX_PER_ROW * NUM_ROWS * NUM_PLANES, testCube, config, NUM_CHANNELS, pinList);
ObjectFLED blanks(7*8*8, blankLeds, config, 7, pinListBlank);

CRGB leds2[PIX_PER_ROW * NUM_ROWS * NUM_PLANES];  // FastLED draw buffer
uint startT=0, stopT=0;
void setup() {
  Serial.begin(STD_OUT_BAUD);
  Serial.print("*********************************************\n");
  Serial.print("* TeensyParallel.ino                        *\n");
  Serial.print("*********************************************\n");
  Serial.printf("CPU speed: %d MHz   Temp: %.1f C  %.1f F   Serial baud: %.1f MHz\n", \
                  F_CPU_ACTUAL / 1000000, \
                  tempmonGetTemp(), tempmonGetTemp() * 9.0 / 5.0 + 32, \
                  800000 * 1.6 / 1000000.0);

  //blank all leds/ all channels

  //Second parallel pin group Teensy 4.0: 10,12,11,13,6,9,32,8,7
  //Third parallel pin group Teensy 4.0: 37, 36, 35, 34, 39, 38, 28, 31, 30 from FastLED docs
//  FastLED.addLeds<NUM_CHANNELS, WS2812, 37, GRB>(leds2, PIX_PER_ROW * NUM_ROWS * NUM_PLANES / NUM_CHANNELS);  //4-strip parallel
//  FastLED.setBrightness(5);
//  FastLED.setCorrection(0xB0E0FF);
  fill_solid(blankLeds[0][0], 7*8*8, 0x0);
  fill_solid(testCube[0][0], NUM_PLANES * NUM_ROWS * PIX_PER_ROW, background);
  leds.begin(1.6, 72);    //1.6 ocervlock factor, 72uS LED latch delay
  leds.setBrightness(6);
  leds.setBalance(0xDAE0FF);

  blanks.begin();         //default timing 800KHz LED clk, 75uS latch
  blanks.show();
  while(Serial.read() == -1);
}  //setup()

void loop() {
  startT = micros();
  // 20 calls
  leds.show();
  leds.show();
  leds.show();
  leds.show();
  leds.show();
  leds.show();
  leds.show();
  leds.show();
  leds.show();
  leds.show();
  leds.show();
  leds.show();
  leds.show();
  leds.show();
  leds.show();
  leds.show();
  leds.show();
  leds.show();
  leds.show();
  leds.show();
  stopT = micros();
  Serial.printf("LEDs/channel:  %d   Serial avg/20 time:  %d uS  %f fps\n", \
                    PIX_PER_ROW*NUM_ROWS*NUM_PLANES/NUM_CHANNELS, 
                  (stopT - startT) / 20, 20000000.0 / (stopT - startT));
  while (Serial.read() == -1);

  startT = micros();
  leds.show();
  stopT = micros();
  Serial.printf("LEDs/channel:  %d   Serial 1 frame time:  %d uS  %f fps\n", \
                    PIX_PER_ROW*NUM_ROWS*NUM_PLANES/NUM_CHANNELS, 
                  (stopT - startT), 1000000.0 / (stopT - startT));
  while (Serial.read() == -1);
  return;
}  //loop()

#endif  //  __IMXRT1062__

