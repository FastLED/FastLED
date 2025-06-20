#include <FastLED.h>

#define NUM_LEDS 60
#define DATA_PIN 3

CRGB leds[NUM_LEDS];

// Create UI groups for organization
UIGroup lightingGroup("Lighting Controls");
UIGroup effectsGroup("Effect Parameters");
UIGroup colorGroup("Color Settings");

// UI elements organized by groups
UISlider brightness("Brightness", 128, 0, 255);
UISlider speed("Speed", 50, 0, 100);
UICheckbox enableEffects("Enable Effects", true);

UISlider hue("Hue", 0, 0, 360);
UISlider saturation("Saturation", 255, 0, 255);

// Create color mode options properly for UIDropdown
fl::Str colorModeOptions[] = {"RGB", "HSV", "Palette"};
UIDropdown colorMode("Color Mode", colorModeOptions, 3);

UINumberField delayTime("Delay (ms)", 50, 0, 1000);
UIButton resetButton("Reset Settings");

void setup() {
    FastLED.addLeds<WS2812, DATA_PIN, GRB>(leds, NUM_LEDS);
    
    // Display group information
    Serial.begin(115200);
    Serial.println("FastLED UIGroup Example");
    Serial.println("======================");
    
    // Print group names to demonstrate string conversion
    Serial.print("Lighting Group: ");
    Serial.println(lightingGroup.name().c_str());
    
    Serial.print("Effects Group: ");
    Serial.println(effectsGroup.name().c_str());
    
    Serial.print("Color Group: ");
    Serial.println(colorGroup.name().c_str());
    
    // Set up button callback
    resetButton.onClicked([]() {
        brightness = 128;
        speed = 50;
        hue = 0;
        saturation = 255;
        delayTime = 50;
        Serial.println("Settings reset!");
    });
}

void loop() {
    // Simple rainbow effect using grouped UI controls
    static uint8_t startIndex = 0;
    
    if (enableEffects) {
        // Use the grouped UI values for the effect
        fill_rainbow(leds, NUM_LEDS, startIndex, 255 / NUM_LEDS);
        // Fix division ambiguity by explicitly casting speed to int
        startIndex += (int)speed / 10;
    } else {
        // Solid color based on grouped color controls
        CHSV color((int)hue, (int)saturation, (int)brightness);
        fill_solid(leds, NUM_LEDS, color);
    }
    
    FastLED.setBrightness((int)brightness);
    FastLED.show();
    // Fix delay ambiguity by explicitly casting to int
    delay((int)delayTime);
}
