#include <FastLED.h>

#define NUM_LEDS 10
#define DATA_PIN 6

CRGB leds[NUM_LEDS];

// Create dropdown with array of options
fl::Str options[] = {"Red", "Green", "Blue", "White", "Rainbow"};
fl::UIDropdown colorDropdown("Color", options, 5);

// Create dropdown with vector of options
fl::vector<fl::Str> patternOptions;
fl::UIDropdown patternDropdown("Pattern", patternOptions);

// On platforms with C++11 support, you can also use initializer lists:
// fl::UIDropdown modeDropdown("Mode", {"Solid", "Fade", "Strobe"});

void setup() {
    Serial.begin(115200);
    FastLED.addLeds<WS2812, DATA_PIN, GRB>(leds, NUM_LEDS);
    
    // Initialize pattern options
    patternOptions.push_back(fl::Str("Solid"));
    patternOptions.push_back(fl::Str("Fade"));
    patternOptions.push_back(fl::Str("Strobe"));
    
    // Set initial selections
    colorDropdown.setSelectedIndex(0);   // Red
    patternDropdown.setSelectedIndex(0); // Solid
    
    // Add change callbacks
    colorDropdown.onChanged([](fl::UIDropdown &dropdown) {
        Serial.print("Color changed to: ");
        Serial.print(dropdown.value().c_str());
        Serial.print(" (index ");
        Serial.print(dropdown.value_int());
        Serial.println(")");
    });
    
    patternDropdown.onChanged([](fl::UIDropdown &dropdown) {
        Serial.print("Pattern changed to: ");
        Serial.print(dropdown.value().c_str());
        Serial.print(" (index ");
        Serial.print(dropdown.value_int());
        Serial.println(")");
    });
    
    Serial.println("UIDropdown Example Started");
    Serial.print("Color options: ");
    for(size_t i = 0; i < colorDropdown.getOptionCount(); i++) {
        Serial.print(colorDropdown.getOption(i).c_str());
        if(i < colorDropdown.getOptionCount() - 1) Serial.print(", ");
    }
    Serial.println();
}

void loop() {
    // Get current selections
    fl::Str currentColor = colorDropdown.value();
    fl::Str currentPattern = patternDropdown.value();
    int colorIndex = colorDropdown.value_int();
    
    // Apply color based on selection
    CRGB selectedColor = CRGB::Black;
    if(currentColor == "Red") selectedColor = CRGB::Red;
    else if(currentColor == "Green") selectedColor = CRGB::Green; 
    else if(currentColor == "Blue") selectedColor = CRGB::Blue;
    else if(currentColor == "White") selectedColor = CRGB::White;
    else if(currentColor == "Rainbow") {
        // Rainbow pattern
        for(int i = 0; i < NUM_LEDS; i++) {
            leds[i] = CHSV(i * 255 / NUM_LEDS + millis() / 20, 255, 255);
        }
    }
    
    // Apply pattern if not rainbow
    if(currentColor != "Rainbow") {
        for(int i = 0; i < NUM_LEDS; i++) {
            if(currentPattern == "Solid") {
                leds[i] = selectedColor;
            } else if(currentPattern == "Fade") {
                uint8_t brightness = sin8(millis() / 10);
                leds[i] = selectedColor;
                leds[i].nscale8(brightness);
            } else if(currentPattern == "Strobe") {
                leds[i] = (millis() / 200) % 2 ? selectedColor : CRGB::Black;
            }
        }
    }
    
    FastLED.show();
    FastLED.delay(50);
    
    // Example of using dropdown as int
    static unsigned long lastPrint = 0;
    if(millis() - lastPrint > 5000) {
        Serial.print("Current settings - Color index: ");
        Serial.print(int(colorDropdown));  // Uses operator int()
        Serial.print(", Pattern: ");
        Serial.println(fl::Str(patternDropdown).c_str());  // Uses operator fl::Str()
        lastPrint = millis();
    }
}