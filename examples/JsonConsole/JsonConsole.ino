#include <FastLED.h>
#include "fl/json_console.h"
#include "fl/scoped_ptr.h"

#define NUM_LEDS 60
#define DATA_PIN 2

CRGB leds[NUM_LEDS];

// Create a UI slider for brightness control
fl::UISlider brightness("brightness", 128, 0, 255);

// JsonConsole for interactive control (using scoped_ptr for automatic cleanup)
fl::scoped_ptr<fl::JsonConsole> console;

void setup() {
    Serial.begin(115200);
    while (!Serial) {
        delay(100); // Wait for serial port to connect
    }
    
    FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
    
    // Create JsonConsole with Serial interface
    // In production, this would use Serial.available, Serial.read, Serial.println
    console.reset(new fl::JsonConsole(
        []() -> int { return Serial.available(); },
        []() -> int { return Serial.read(); },
        [](const char* msg) { Serial.println(msg); }
    ));
    
    // Initialize the console
    console->init();
    
    Serial.println("FastLED JsonConsole Example");
    Serial.println("Commands:");
    Serial.println("  brightness: <0-255>   - Set LED brightness by name");
    Serial.println("  <id>: <0-255>        - Set LED brightness by component ID");
    Serial.println("  help                  - Show available commands");
    Serial.println();
    Serial.println("Examples:");
    Serial.println("  brightness: 128       - Set brightness to 128 using name");
    Serial.println("  1: 200                - Set brightness to 200 using ID 1");
    Serial.println();
}

void loop() {
    // Process console input
    if (console) {
        console->update();
    }
    
    // Update LED strip based on brightness slider
    FastLED.setBrightness(brightness.value());
    
    // Simple rainbow pattern
    static uint8_t hue = 0;
    for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = CHSV(hue + (i * 4), 255, 255);
    }
    hue++;
    
    FastLED.show();
    delay(20);
}
