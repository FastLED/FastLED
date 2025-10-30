# Your First Blink Example

Write and run your first LED program with FastLED.

**Difficulty Level**: ⭐ Beginner
**Time to Complete**: 30 minutes (including hardware setup)
**Prerequisites**:
- [FastLED Concepts](concepts.md) - Understanding of basic LED control concepts
- Arduino IDE or PlatformIO installed
- LED strip connected to your microcontroller

**You'll Learn**:
- How to write a complete FastLED program from scratch
- The essential setup() and loop() pattern in action
- How to use `fill_solid()` to set all LEDs to one color
- Why `FastLED.show()` is required to update your LED strip
- How to customize pin numbers, LED counts, and timing for your hardware

## Complete Example

This program blinks all LEDs between red and off:

```cpp
#include <FastLED.h>

// Configuration
#define LED_PIN     5
#define NUM_LEDS    60
#define LED_TYPE    WS2812B
#define COLOR_ORDER GRB

// LED array
CRGB leds[NUM_LEDS];

void setup() {
    // Initialize FastLED
    FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
    FastLED.setBrightness(50);  // Start with low brightness
}

void loop() {
    // Turn all LEDs red
    fill_solid(leds, NUM_LEDS, CRGB::Red);
    FastLED.show();
    delay(1000);

    // Turn all LEDs off
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    FastLED.show();
    delay(1000);
}
```

## How It Works

### Setup Phase

```cpp
void setup() {
    FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
    FastLED.setBrightness(50);
}
```

- `FastLED.addLeds<>()` - Tells FastLED about your hardware
- `FastLED.setBrightness(50)` - Sets brightness to ~20% (safe for testing)

### Loop Phase

```cpp
void loop() {
    fill_solid(leds, NUM_LEDS, CRGB::Red);  // Set all LEDs to red
    FastLED.show();                          // Send to strip
    delay(1000);                             // Wait 1 second

    fill_solid(leds, NUM_LEDS, CRGB::Black); // Set all LEDs to off
    FastLED.show();                          // Send to strip
    delay(1000);                             // Wait 1 second
}
```

## Customization

### Change the Pin

```cpp
#define LED_PIN     6  // Use pin 6 instead of 5
```

### Change Number of LEDs

```cpp
#define NUM_LEDS    30  // For a 30-LED strip
```

### Change Color

```cpp
fill_solid(leds, NUM_LEDS, CRGB::Blue);   // Blue instead of red
fill_solid(leds, NUM_LEDS, CRGB::Green);  // Green
fill_solid(leds, NUM_LEDS, CRGB::Purple); // Purple
```

### Change Blink Speed

```cpp
delay(500);   // Faster (0.5 seconds)
delay(2000);  // Slower (2 seconds)
```

## Troubleshooting

### LEDs Don't Light Up

1. **Check power connections** - Verify 5V and GND are connected
2. **Check data pin** - Ensure `LED_PIN` matches your wiring
3. **Try different brightness** - Increase to `FastLED.setBrightness(100)`
4. **Verify LED type** - Confirm `LED_TYPE` matches your strip

### Wrong Colors

Try different `COLOR_ORDER` values:

```cpp
#define COLOR_ORDER GRB  // Try GRB first (most common)
// If that doesn't work, try:
#define COLOR_ORDER RGB
#define COLOR_ORDER BGR
```

### Only First LED Works

- Add a 220-470Ω resistor on the data line
- Check for damaged LEDs in your strip

## What's Next?

### More Color Options

```cpp
// Named colors
CRGB::Red
CRGB::Green
CRGB::Blue
CRGB::White
CRGB::Purple
CRGB::Orange
CRGB::Yellow

// Custom RGB colors
CRGB(255, 0, 0)     // Red
CRGB(0, 255, 0)     // Green
CRGB(128, 0, 128)   // Purple

// HSV colors
CHSV(0, 255, 255)    // Red
CHSV(96, 255, 255)   // Green
CHSV(160, 255, 255)  // Blue
```

### Individual LED Control

```cpp
void loop() {
    // Turn on first 3 LEDs to different colors
    leds[0] = CRGB::Red;
    leds[1] = CRGB::Green;
    leds[2] = CRGB::Blue;

    // Rest of LEDs stay off
    FastLED.show();
    delay(1000);
}
```

### Fill a Range

```cpp
// Fill LEDs 10-19 with green
fill_solid(&leds[10], 10, CRGB::Green);
```

## Next Steps

Now that you have a working program:

- [Core Concepts](../core-concepts/) - Learn about data structures and timing
- [Basic Patterns](../basic-patterns/) - Create your first animations
