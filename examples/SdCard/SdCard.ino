


#ifdef __AVR__
void setup() {
  // put your setup code here, to run once:
}

void loop() {
  // put your main code here, to run repeatedly:
}
#else

#include <FastLED.h>
#include "Arduino.h"
#include "file_system.h"

const int CHIP_SELECT_PIN = 5;

Fs fs(CHIP_SELECT_PIN);


void setup() {
    Serial.begin(115200);
    if (!fs.begin()) {
        Serial.println("Failed to initialize file system.");
    }
    delay(2000);  // If something ever goes wrong this delay will allow upload.
}

void loop() {
    FileHandleRef fh = fs.openRead("data/video.dat");
    if (!fh) {
      Serial.println("Failed to open SD card because sd is null");
    } else {
      fs.close(fh);
    }
    delay(1000);    
}

#endif