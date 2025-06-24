#include "FastLED.h"

#if defined(__AVR__)
#include "avr_test.h"
#elif defined(ESP32)
#include "esp_test.h"
#endif


void setup() {
    Serial.begin(115200);
    Serial.println("Setup");
}

void loop() {
    Serial.println("Loop");
    delay(100);    
}