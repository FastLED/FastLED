/// @file    Noise.ino
/// @brief   Demonstrates how to use noise generation on a 2D LED matrix
/// @example Noise.ino


#include <FastLED.h>

#include "fx/storage/sd.hpp"
#include <iostream>

#define SCALE 20
#define SPEED 30


void runSdCardTest() {
    #if 0
    SdCardSpi sdCard;
    if (sdCard.isCompiledIn()) {
        // Serial.println("SD card support compiled in");
        cout << "SD card support compiled in" << endl;
    } else {
        // Serial.println("No SD card support compiled in");
        cout << "No SD card support compiled in" << endl;
    }
    #endif
}


void setup() {
    delay(1000); // sanity delay
    runSdCardTest();

}



void loop() {
    
}
