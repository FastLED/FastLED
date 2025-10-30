# Chases and Scanners

**Difficulty Level**: ⭐ Beginner
**Time to Complete**: 25-30 minutes
**Prerequisites**: [Animations](animations.md), [Timing Concepts](../core-concepts/timing.md), [Solid Colors](solid-colors.md)

**You'll Learn**:
- How to create movement along your LED strip using position tracking
- The difference between bouncing (bidirectional) and chase (unidirectional) effects
- How to use `fadeToBlackBy()` to create trailing effects behind moving LEDs
- How to implement boundary detection and direction reversal for bouncing patterns
- How to create variations like multiple scanners, color changes, and variable width

Chase and scanner effects create the illusion of movement along your LED strip. The classic "Cylon" or "Knight Rider" scanner is an iconic example that teaches essential animation techniques.

## Nightrider / Cylon Effect

The Cylon effect features a LED that bounces back and forth with a fading trail:

```cpp
void cylon() {
    static uint8_t pos = 0;
    static int8_t direction = 1;

    // Fade all LEDs
    fadeToBlackBy(leds, NUM_LEDS, 20);

    // Set current position
    leds[pos] = CRGB::Red;

    // Move position
    pos += direction;
    if (pos == 0 || pos == NUM_LEDS - 1) {
        direction = -direction;
    }
}
```

### How It Works

1. **Position Tracking**: `pos` tracks the current LED position
2. **Direction Control**: `direction` determines forward (1) or backward (-1) movement
3. **Trail Effect**: `fadeToBlackBy()` creates a trailing fade
4. **Boundary Detection**: When reaching either end, direction reverses
5. **LED Illumination**: Current position is set to the desired color

## Key Concepts

### Trail Effects with fadeToBlackBy

```cpp
fadeToBlackBy(leds, NUM_LEDS, 20);
```

This function fades all LEDs toward black by the specified amount (0-255):
- **20**: Slower fade, longer trail
- **100**: Faster fade, shorter trail
- **255**: No trail (instant off)

### Position Management

Track and update position with boundaries:

```cpp
pos += direction;
if (pos == 0 || pos == NUM_LEDS - 1) {
    direction = -direction;
}
```

This creates a bouncing effect at strip ends.

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

void cylon() {
    static uint8_t pos = 0;
    static int8_t direction = 1;

    // Fade all LEDs
    fadeToBlackBy(leds, NUM_LEDS, 20);

    // Set current position
    leds[pos] = CRGB::Red;

    // Move position
    pos += direction;
    if (pos == 0 || pos == NUM_LEDS - 1) {
        direction = -direction;
    }
}

void loop() {
    cylon();
    FastLED.show();
    delay(30);  // Control speed
}
```

## Variations

### Multiple Dots

Create multiple scanning dots:

```cpp
void multiCylon() {
    static uint8_t pos1 = 0;
    static uint8_t pos2 = NUM_LEDS / 2;
    static int8_t direction1 = 1;
    static int8_t direction2 = -1;

    fadeToBlackBy(leds, NUM_LEDS, 20);

    // First scanner
    leds[pos1] = CRGB::Red;
    pos1 += direction1;
    if (pos1 == 0 || pos1 == NUM_LEDS - 1) {
        direction1 = -direction1;
    }

    // Second scanner
    leds[pos2] = CRGB::Blue;
    pos2 += direction2;
    if (pos2 == 0 || pos2 == NUM_LEDS - 1) {
        direction2 = -direction2;
    }
}
```

### Color Changing Scanner

Cycle through colors as it scans:

```cpp
void colorCylon() {
    static uint8_t pos = 0;
    static int8_t direction = 1;
    static uint8_t hue = 0;

    fadeToBlackBy(leds, NUM_LEDS, 20);

    leds[pos] = CHSV(hue, 255, 255);

    pos += direction;
    if (pos == 0 || pos == NUM_LEDS - 1) {
        direction = -direction;
        hue += 32;  // Change color at each bounce
    }
}
```

### Simple Chase (One Direction)

A dot that continuously moves in one direction:

```cpp
void chase() {
    static uint8_t pos = 0;

    fadeToBlackBy(leds, NUM_LEDS, 30);
    leds[pos] = CRGB::Green;

    pos++;
    if (pos >= NUM_LEDS) {
        pos = 0;
    }
}
```

### Wider Scanner

Create a wider scanning effect:

```cpp
void wideCylon() {
    static uint8_t pos = 0;
    static int8_t direction = 1;

    fadeToBlackBy(leds, NUM_LEDS, 20);

    // Light up multiple LEDs for width
    int width = 5;
    for (int i = 0; i < width; i++) {
        int ledPos = pos + i - (width / 2);
        if (ledPos >= 0 && ledPos < NUM_LEDS) {
            leds[ledPos] = CRGB::Red;
        }
    }

    pos += direction;
    if (pos == 0 || pos == NUM_LEDS - 1) {
        direction = -direction;
    }
}
```

## Advanced Techniques

### Variable Speed

Change speed based on position:

```cpp
void acceleratingCylon() {
    static uint8_t pos = 0;
    static int8_t direction = 1;
    static uint8_t speed = 1;

    fadeToBlackBy(leds, NUM_LEDS, 20);
    leds[pos] = CRGB::Red;

    // Calculate speed based on distance from center
    uint8_t distFromCenter = abs(pos - NUM_LEDS / 2);
    speed = map(distFromCenter, 0, NUM_LEDS / 2, 1, 3);

    pos += direction * speed;
    if (pos <= 0 || pos >= NUM_LEDS - 1) {
        direction = -direction;
        pos = constrain(pos, 0, NUM_LEDS - 1);
    }
}
```

## Tips

- Adjust `fadeToBlackBy()` value to control trail length (20-50 works well)
- Use `delay()` to control scan speed (20-50ms is typical)
- Experiment with different colors for different moods
- Multiple scanners can create interesting collision effects
- Try using `blur1d()` instead of `fadeToBlackBy()` for smoother trails

## Related Examples

See the full [Cylon example](../../examples/Cylon) in the FastLED repository for more details.

## Next Steps

Explore [Rainbow Effects](rainbow.md) to learn how to create smooth color cycling patterns using HSV color space.
