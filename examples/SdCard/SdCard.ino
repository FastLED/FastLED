

#include <FastLED.h>
#include "Arduino.h"
#include "fx/storage/sd.h"

#define SPI_DATA 13
#define SPI_CLOCK 14

using namespace storage;

SdCardSpi sd(SPI_CLOCK, SPI_DATA);

#define INVALID_FILENAME "fhjdiskljdskj.txt"

void setup() {
    Serial.begin(115200);
    sd.begin();
    delay(2000);  // If something ever goes wrong this delay will allow upload.
}

void loop() {
    FileHandle *file = sd.open(INVALID_FILENAME, 0);
    sd.close(file);
    delay(1000);    
}
