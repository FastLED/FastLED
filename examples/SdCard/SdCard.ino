


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
#include "fx/storage/fs.hpp"

const int CHIP_SELECT_PIN = 5;

FsPtr fs = FsPtr::New(CHIP_SELECT_PIN);

#define INVALID_FILENAME "fhjdiskljdskj.txt"

void setup() {
    Serial.begin(115200);
    if (fs) {
        fs->begin();
    } else {
        Serial.println("Failed to initialize SD card because sd is null");
    }

    delay(2000);  // If something ever goes wrong this delay will allow upload.
}

void loop() {
    if (fs) {
      FileHandlePtr file = fs->openRead(INVALID_FILENAME);
      fs->close(file);
    } else {
        Serial.println("Failed to open SD card because sd is null");
    }

    delay(1000);    
}

#endif