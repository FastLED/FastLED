# Rainbow Effects

**Difficulty Level**: ⭐ Beginner
**Time to Complete**: 30-35 minutes
**Prerequisites**:
- [Solid Colors](solid-colors.md) - Understanding fill functions and color basics
- [Animations](animations.md) - Working with static variables for animation
- [Color Theory](../core-concepts/color-theory.md) - RGB vs HSV color spaces

**You'll Learn**:
- How to use `fill_rainbow()` to create smooth rainbow gradients across your LED strip
- Why HSV color space makes rainbow effects dramatically simpler than RGB
- How to animate rainbows by cycling the starting hue value
- How to control rainbow density, speed, and direction with parameters
- Advanced rainbow techniques including mirroring, chunking, breathing effects, and dual-direction animations

Rainbow effects showcase the power of HSV color space for creating smooth, natural color transitions. These patterns are visually striking and demonstrate essential color cycling techniques.

## Static Rainbow

Create a rainbow that spans your entire LED strip:

```cpp
void rainbow() {
    fill_rainbow(leds, NUM_LEDS, 0, 255 / NUM_LEDS);
}
```

### Parameters

- `leds`: Your LED array
- `NUM_LEDS`: Number of LEDs to fill
- `0`: Starting hue (0-255)
- `255 / NUM_LEDS`: Hue increment per LED

This distributes all 256 hue values evenly across your strip.

## Moving Rainbow

Animate the rainbow by cycling the starting hue:

```cpp
void movingRainbow() {
    static uint8_t hue = 0;
    fill_rainbow(leds, NUM_LEDS, hue, 255 / NUM_LEDS);
    hue++;
}
```

The rainbow appears to move along the strip as the starting hue increments each frame.

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

void movingRainbow() {
    static uint8_t hue = 0;
    fill_rainbow(leds, NUM_LEDS, hue, 255 / NUM_LEDS);
    hue++;
}

void loop() {
    movingRainbow();
    FastLED.show();
    delay(20);
}
```

## Understanding HSV Color Space

HSV uses three components:
- **Hue** (0-255): Color around the color wheel
- **Saturation** (0-255): Color intensity (255 = vivid, 0 = white)
- **Value** (0-255): Brightness (255 = bright, 0 = off)

```cpp
CHSV color = CHSV(hue, 255, 255);  // Full saturation and brightness
```

### Hue Values

- 0: Red
- 32: Orange
- 64: Yellow
- 96: Green
- 128: Cyan
- 160: Blue
- 192: Purple
- 224: Pink
- 255: Red (wraps around)

## Variations

### Tighter Rainbow

Create multiple rainbow cycles across the strip:

```cpp
void tightRainbow() {
    static uint8_t hue = 0;
    fill_rainbow(leds, NUM_LEDS, hue, 5);  // Larger delta = tighter rainbow
    hue++;
}
```

### Reverse Direction

Make the rainbow move backward:

```cpp
void reverseRainbow() {
    static uint8_t hue = 0;
    fill_rainbow(leds, NUM_LEDS, hue, 255 / NUM_LEDS);
    hue--;  // Decrement instead of increment
}
```

### Variable Speed

Control rainbow animation speed:

```cpp
void speedRainbow(uint8_t speed) {
    static uint8_t hue = 0;
    fill_rainbow(leds, NUM_LEDS, hue, 255 / NUM_LEDS);
    hue += speed;  // Higher speed = faster movement
}

void loop() {
    speedRainbow(3);  // 3x faster than normal
    FastLED.show();
    delay(20);
}
```

### Partial Rainbow

Fill only a portion of the strip:

```cpp
void partialRainbow() {
    static uint8_t hue = 0;

    // Rainbow on first half
    fill_rainbow(&leds[0], NUM_LEDS / 2, hue, 10);

    // Solid color on second half
    fill_solid(&leds[NUM_LEDS / 2], NUM_LEDS / 2, CRGB::Black);

    hue++;
}
```

### Mirrored Rainbow

Create a mirrored rainbow effect:

```cpp
void mirrorRainbow() {
    static uint8_t hue = 0;

    int half = NUM_LEDS / 2;
    fill_rainbow(&leds[0], half, hue, 255 / half);

    // Mirror to second half
    for (int i = 0; i < half; i++) {
        leds[NUM_LEDS - 1 - i] = leds[i];
    }

    hue++;
}
```

## Advanced Techniques

### Breathing Rainbow

Combine rainbow with brightness pulsing:

```cpp
void breathingRainbow() {
    static uint8_t hue = 0;

    // Create rainbow
    fill_rainbow(leds, NUM_LEDS, hue, 255 / NUM_LEDS);

    // Pulse brightness
    uint8_t brightness = beatsin8(12, 50, 255);  // 12 BPM, 50-255 range
    FastLED.setBrightness(brightness);

    hue++;
}
```

### Chunked Rainbow

Create discrete rainbow blocks:

```cpp
void chunkRainbow() {
    static uint8_t hue = 0;
    int chunkSize = 5;

    for (int i = 0; i < NUM_LEDS; i += chunkSize) {
        fill_solid(&leds[i], chunkSize, CHSV(hue + (i * 10), 255, 255));
    }

    hue++;
}
```

### Dual Direction Rainbow

Two rainbows moving in opposite directions:

```cpp
void dualRainbow() {
    static uint8_t hue1 = 0;
    static uint8_t hue2 = 128;  // Offset by half

    // First rainbow
    for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = CHSV(hue1 + (i * 4), 255, 128);
    }

    // Second rainbow (additive)
    for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] += CHSV(hue2 - (i * 4), 255, 128);
    }

    hue1++;
    hue2--;
}
```

## Tips

- Use `fill_rainbow()` for efficient rainbow generation
- HSV makes color cycling much easier than RGB
- Adjust the hue delta (4th parameter) to control rainbow tightness
- Combine with other effects like fading or trails for variety
- Use `beatsin8()` for smooth, automatic speed variations

## Why HSV for Rainbows?

RGB rainbow requires complex calculations:
```cpp
// RGB rainbow (complex)
if (hue < 85) {
    r = hue * 3;
    g = 255 - hue * 3;
    b = 0;
} // ... more cases
```

HSV rainbow is simple:
```cpp
// HSV rainbow (simple)
CHSV color = CHSV(hue, 255, 255);
hue++;
```

## Next Steps

You've completed the basic patterns! Now explore:
- [Color Palettes](../../COOKBOOK.md#color-palettes) - Pre-defined color schemes
- [Noise and Perlin Noise](../../COOKBOOK.md#noise-and-perlin-noise) - Organic animations
- [Common Recipes](../../COOKBOOK.md#common-recipes) - Popular effects like fire and twinkle
