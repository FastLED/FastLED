# Simple Animations

**Difficulty Level**: ⭐ Beginner
**Time to Complete**: 20-25 minutes
**Prerequisites**: [Solid Colors](solid-colors.md), [Timing Concepts](../core-concepts/timing.md)

**You'll Learn**:
- How to create smooth fade in/out animations using brightness control
- The power of static variables for maintaining animation state
- The difference between global and per-LED brightness control
- How to control animation speed with timing and delay
- How to create variations by parameterizing your animation functions

Learn how to create smooth, dynamic effects by animating brightness and color over time. The fade in/out effect is one of the most fundamental animations in LED programming.

## Fade In/Out Effect

This classic effect smoothly fades LEDs from off to full brightness and back again.

```cpp
void fadeInOut() {
    static uint8_t brightness = 0;
    static int8_t direction = 1;

    brightness += direction;
    if (brightness == 0 || brightness == 255) {
        direction = -direction;
    }

    fill_solid(leds, NUM_LEDS, CRGB::Purple);
    FastLED.setBrightness(brightness);
    FastLED.show();
}
```

### How It Works

1. **Static Variables**: `brightness` and `direction` persist between function calls
2. **Direction Control**: `direction` is either 1 (increasing) or -1 (decreasing)
3. **Boundary Detection**: When brightness reaches 0 or 255, direction reverses
4. **Brightness Application**: `setBrightness()` affects all LEDs globally

## Key Concepts

### Static Variables

Static variables retain their values between function calls:

```cpp
static uint8_t brightness = 0;  // Initialized once, persists
```

Without `static`, the variable would reset to 0 every time the function is called.

### Global vs. Per-LED Brightness

```cpp
// Global brightness (affects all LEDs)
FastLED.setBrightness(brightness);

// Per-LED brightness (using HSV)
for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CHSV(160, 255, brightness);
}
```

### Timing Control

Control animation speed by calling the function at different rates:

```cpp
void loop() {
    fadeInOut();
    delay(20);  // Faster animation

    // Or use timing macros
    EVERY_N_MILLISECONDS(20) {
        fadeInOut();
    }
}
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

void fadeInOut() {
    static uint8_t brightness = 0;
    static int8_t direction = 1;

    brightness += direction;
    if (brightness == 0 || brightness == 255) {
        direction = -direction;
    }

    fill_solid(leds, NUM_LEDS, CRGB::Purple);
    FastLED.setBrightness(brightness);
    FastLED.show();
}

void loop() {
    fadeInOut();
    delay(10);  // Control speed here
}
```

## Variations

### Different Colors

```cpp
void fadeInOutColor(CRGB color) {
    static uint8_t brightness = 0;
    static int8_t direction = 1;

    brightness += direction;
    if (brightness == 0 || brightness == 255) {
        direction = -direction;
    }

    fill_solid(leds, NUM_LEDS, color);
    FastLED.setBrightness(brightness);
    FastLED.show();
}

// Usage
void loop() {
    fadeInOutColor(CRGB::Red);
    delay(10);
}
```

### Per-LED Fading

For more control, fade individual LEDs:

```cpp
void fadeInOutPerLED() {
    static uint8_t brightness = 0;
    static int8_t direction = 1;

    brightness += direction;
    if (brightness == 0 || brightness == 255) {
        direction = -direction;
    }

    for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = CHSV(160, 255, brightness);
    }
    FastLED.show();
}
```

### Variable Speed

Adjust fade speed dynamically:

```cpp
void fadeInOutSpeed(uint8_t speed) {
    static uint8_t brightness = 0;
    static int8_t direction = speed;

    brightness += direction;
    if (brightness == 0 || brightness >= 255) {
        direction = -direction;
    }

    fill_solid(leds, NUM_LEDS, CRGB::Blue);
    FastLED.setBrightness(brightness);
    FastLED.show();
}
```

## Tips

- Use `delay()` or timing macros to control animation speed
- Static variables are essential for maintaining state between calls
- Global brightness is more efficient than per-LED brightness changes
- Experiment with different increment values for faster/slower fades
- Combine with color cycling for more complex effects

## Next Steps

Now that you understand basic animations, explore [Chases and Scanners](chases.md) to learn about moving patterns with trail effects.
