/// THIS IS A PLACEHOLDER FOR THE ESP32 I2S DEMO

#include "third_party/yves/I2SClockLessLedDriveresp32s3/driver.h"

I2SClocklessLedDriveresp32S3 driver;
#define NUMSTRIPS 16
#define NUM_LEDS_PER_STRIP 256

#define EXAMPLE_PIN_NUM_DATA0 19  // B0
#define EXAMPLE_PIN_NUM_DATA1 45  // B1
#define EXAMPLE_PIN_NUM_DATA2 21  // B2
#define EXAMPLE_PIN_NUM_DATA3 6   // B3
#define EXAMPLE_PIN_NUM_DATA4 7   // B4
#define EXAMPLE_PIN_NUM_DATA5 8   // G0
#define EXAMPLE_PIN_NUM_DATA6 9   // G1
#define EXAMPLE_PIN_NUM_DATA7 10  // G2
#define EXAMPLE_PIN_NUM_DATA8 11  // G3
#define EXAMPLE_PIN_NUM_DATA9 12  // G4
#define EXAMPLE_PIN_NUM_DATA10 13 // G5
#define EXAMPLE_PIN_NUM_DATA11 14 // R0
#define EXAMPLE_PIN_NUM_DATA12 15 // R1
#define EXAMPLE_PIN_NUM_DATA13 16 // R2
#define EXAMPLE_PIN_NUM_DATA14 17 // R3
#define EXAMPLE_PIN_NUM_DATA15 18 // R4

 int pins[] = {
        EXAMPLE_PIN_NUM_DATA0,
        EXAMPLE_PIN_NUM_DATA1,
        EXAMPLE_PIN_NUM_DATA2,
        EXAMPLE_PIN_NUM_DATA3,
        EXAMPLE_PIN_NUM_DATA4,
        EXAMPLE_PIN_NUM_DATA5,
        EXAMPLE_PIN_NUM_DATA6,
        EXAMPLE_PIN_NUM_DATA7,
        EXAMPLE_PIN_NUM_DATA8,
        EXAMPLE_PIN_NUM_DATA9,
        EXAMPLE_PIN_NUM_DATA10,
        EXAMPLE_PIN_NUM_DATA11,
        EXAMPLE_PIN_NUM_DATA12,
        EXAMPLE_PIN_NUM_DATA13,
        EXAMPLE_PIN_NUM_DATA14,
        EXAMPLE_PIN_NUM_DATA15
 };
I2SClocklessLedDriveresp32S3 driver;

#define NUM_LEDS (NUM_LEDS_PER_STRIP * NUMSTRIPS)


CRGB leds[NUM_LEDS];
void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
 driver.initled((uint8_t *)leds, pins, NUMSTRIPS, NUM_LEDS_PER_STRIP);
 driver.setBrightness(32);

}
int off=0;

void loop() {
  
    for(int j=0;j<NUMSTRIPS;j++)
    {
        
        for(int i=0;i<NUM_LEDS_PER_STRIP;i++)
        {
            
           leds[(i+off)%NUM_LEDS_PER_STRIP+NUM_LEDS_PER_STRIP*j]=CHSV(i,255,255);
            
        }
        for(int i=0;i<j+1;i++)
        {
            
           leds[i%NUM_LEDS_PER_STRIP+NUM_LEDS_PER_STRIP*j]=CRGB::White;
            
        }
    }
    driver.show();
    off++;
}