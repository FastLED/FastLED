/// @file    Noise.ino
/// @brief   Demonstrates how to use noise generation on a 2D LED matrix
/// @example Noise.ino


#include <FastLED.h>

#include "fx/storage/sd.hpp"
#include <iostream>

using namespace std;

#define SCALE 20
#define SPEED 30
#define CS_PIN 5

SdCardSpiPtr SD_CARD_READER = SdCardSpiPtr::New(CS_PIN);


void runSdCardTest() {
    cout << "Running SD card test" << endl;
    SD_CARD_READER->begin(CS_PIN);
    FileHandlePtr file = SD_CARD_READER->openRead("/test.txt");
    if (!file) {
        cout << "Failed to open file" << endl;
        return;
    }
    cout << "File opened" << endl;
    char buffer[256];
    size_t bytesRead = file->read((uint8_t*)buffer, sizeof(buffer));
    cout << "Read " << bytesRead << " bytes" << endl;
    cout << "File contents: " << buffer << endl;
    file->close();
    cout << "File closed" << endl;
    SD_CARD_READER->end();
    cout << "SD card test complete" << endl;
}


void setup() {
    delay(1000); // sanity delay
}



void loop() {
    runSdCardTest();
    delay(1000);
}
