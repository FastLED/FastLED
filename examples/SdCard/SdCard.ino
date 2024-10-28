


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
#include "fx/storage/sd.hpp"

SdCardSpiPtr sd = SdCardSpiPtr::New(5);

#define INVALID_FILENAME "fhjdiskljdskj.txt"

void setup() {
    Serial.begin(115200);
    if (sd) {
        sd->begin(5);
    } else {
        Serial.println("Failed to initialize SD card because sd is null");
    }

    delay(2000);  // If something ever goes wrong this delay will allow upload.
}

void loop() {
    if (sd) {
      FileHandlePtr file = sd->openRead(INVALID_FILENAME);
      sd->close(file);
    } else {
        Serial.println("Failed to open SD card because sd is null");
    }

    delay(1000);    
}

#endif