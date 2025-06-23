#include "FastLED.h"

#if defined(__AVR__)
#include "avr_test.h"
#elif defined(ESP8266)
#include "esp8266_test.h"
#endif


void setup() {
    Serial.begin(115200);
    Serial.println("Setup");
}

void loop() {
    Serial.println("Loop");
    delay(100);    
}