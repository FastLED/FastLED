


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
    sd->begin(5);
    delay(2000);  // If something ever goes wrong this delay will allow upload.
}

void loop() {
    FileHandlePtr file = sd->openRead(INVALID_FILENAME);
    sd->close(file);
    delay(1000);    
}

#endif