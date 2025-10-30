# Solid Colors

**Difficulty Level**: ⭐ Beginner
**Time to Complete**: 15-20 minutes
**Prerequisites**: [Core Concepts](../core-concepts/README.md), [First Example](../getting-started/first-example.md)

**You'll Learn**:
- How to fill your entire LED strip with a single solid color
- The difference between RGB and HSV color spaces and when to use each
- How to use FastLED's efficient fill functions instead of manual loops
- How to fill specific sections of your LED strip with different colors
- Named color constants for readable, maintainable code

Solid colors are the foundation of LED control. This pattern demonstrates how to fill your LED strip with a single color using FastLED's efficient fill functions.

## Basic Usage

### Fill Entire Strip

Fill all LEDs with a single color:

```cpp
// Fill entire strip
fill_solid(leds, NUM_LEDS, CRGB::Blue);
```

### Fill a Range

Fill a specific section of your strip:

```cpp
// Fill range
fill_solid(&leds[10], 20, CRGB::Green);  // LEDs 10-29
```

This fills 20 LEDs starting from index 10, effectively controlling LEDs 10 through 29.

### Fill with HSV

Use HSV color space for more intuitive color selection:

```cpp
// Fill with HSV
fill_solid(leds, NUM_LEDS, CHSV(160, 255, 255));
```

## Understanding Color Spaces

### RGB Colors

RGB (Red, Green, Blue) uses three channels from 0-255:

```cpp
CRGB color = CRGB(255, 0, 0);      // Red
CRGB color = CRGB(0, 255, 0);      // Green
CRGB color = CRGB(0, 0, 255);      // Blue
CRGB color = CRGB(255, 255, 255);  // White
```

### HSV Colors

HSV (Hue, Saturation, Value) is often more intuitive:

```cpp
CHSV hsv = CHSV(0, 255, 255);      // Red
CHSV hsv = CHSV(96, 255, 255);     // Green
CHSV hsv = CHSV(160, 255, 255);    // Blue

// Convert HSV to RGB (automatic)
leds[0] = hsv;
```

**Why use HSV?**
- Easier to create smooth color transitions (just increment hue)
- Natural for rainbow effects
- Intuitive control of brightness without changing color

## Named Colors

FastLED provides convenient named colors:

```cpp
leds[0] = CRGB::Red;
leds[1] = CRGB::Green;
leds[2] = CRGB::Blue;
leds[3] = CRGB::White;
leds[4] = CRGB::Yellow;
leds[5] = CRGB::Purple;
leds[6] = CRGB::Cyan;
leds[7] = CRGB::Black;  // Off
```

## Complete Example

```cpp
#include <FastLED.h>

#define LED_PIN     5
#define NUM_LEDS    60
#define LED_TYPE    WS2812B
#define COLOR_ORDER GRB

CRGB leds[NUM_LEDS];

void setup() {
    FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
    FastLED.setBrightness(50);
}

void loop() {
    // Cycle through different solid colors
    fill_solid(leds, NUM_LEDS, CRGB::Red);
    FastLED.show();
    delay(1000);

    fill_solid(leds, NUM_LEDS, CRGB::Green);
    FastLED.show();
    delay(1000);

    fill_solid(leds, NUM_LEDS, CRGB::Blue);
    FastLED.show();
    delay(1000);

    // Fill different sections
    fill_solid(&leds[0], 20, CRGB::Red);
    fill_solid(&leds[20], 20, CRGB::Green);
    fill_solid(&leds[40], 20, CRGB::Blue);
    FastLED.show();
    delay(1000);
}
```

## Tips

- Use `fill_solid()` instead of manual loops for better performance
- HSV color space makes it easier to create smooth color transitions
- Use `FastLED.clear()` as a shortcut for filling with black (off)
- Named colors like `CRGB::Red` are more readable than `CRGB(255, 0, 0)`

## Next Steps

Once you're comfortable with solid colors, move on to [Animations](animations.md) to learn how to create dynamic effects.
