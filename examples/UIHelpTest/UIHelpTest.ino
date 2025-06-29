/// @file    UIHelpTest.ino
/// @brief   Test example for UIHelp component
/// @example UIHelpTest.ino

#include <Arduino.h>
#include <FastLED.h>

// Simple test to verify UIHelp component works
#define NUM_LEDS 16
#define LED_PIN 2

CRGB leds[NUM_LEDS];

// Create help documentation using markdown
UIHelp helpBasic(R"(# FastLED UIHelp Test

This example demonstrates the **UIHelp** component functionality.

## Features

- **Markdown support** for rich text formatting
- `Code blocks` for technical documentation  
- *Italic text* and **bold text**
- Support for [links](https://fastled.io)

## Usage

```cpp
// Basic usage
UIHelp help("Your markdown content here");

// With grouping
UIHelp help("Content");
help.setGroup("Documentation");
```

### Supported Markdown

1. Headers (H1, H2, H3)
2. Bold and italic text
3. Code blocks and inline code
4. Lists (ordered and unordered)
5. Links

Visit [FastLED Documentation](https://github.com/FastLED/FastLED/wiki) for more examples.)");

UIHelp helpAdvanced(R"(## Advanced FastLED Usage

### Color Management

FastLED provides several ways to work with colors:

- `CRGB` for RGB colors
- `CHSV` for HSV colors  
- `ColorFromPalette()` for palette-based colors

```cpp
// Set pixel colors
leds[0] = CRGB::Red;
leds[1] = CHSV(160, 255, 255); // Blue-green
leds[2] = ColorFromPalette(RainbowColors_p, 64);
```

### Animation Patterns

Common animation techniques:

1. **Fading**: Use `fadeToBlackBy()` or `fadeLightBy()`
2. **Color cycling**: Modify HSV hue values
3. **Movement**: Shift array contents or use math functions

### Performance Tips

- Call `FastLED.show()` only when needed
- Use `FASTLED_DELAY()` instead of `delay()`
- Consider using *fast math* functions for smoother animations)");

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("UIHelp Test Example");
    Serial.println("==================");
    
    // Initialize LEDs
    FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(50);
    
    // Set up help groupings
    helpBasic.setGroup("Getting Started");
    //helpAdvanced.setGroup("Advanced Topics");
    
    Serial.println("UIHelp components created successfully!");
    Serial.println("Check the web interface for rendered help documentation.");
}

void loop() {
    // Simple rainbow animation for visual feedback
    static uint8_t hue = 0;
    fill_rainbow(leds, NUM_LEDS, hue++, 7);
    FastLED.show();
    delay(50);
} 
