# Blending and Transitions

**Difficulty Level**: ⭐⭐ Intermediate
**Time to Complete**: 40-50 minutes
**Prerequisites**:
- [Basic Patterns - Animations](../basic-patterns/animations.md) - Understanding static variables and timing
- [Color Theory](../core-concepts/color-theory.md) - RGB and HSV color models
- [Timing and Animation Control](../core-concepts/timing.md) - Frame rate and timing concepts

**You'll Learn**:
- How to smoothly blend between colors using `blend()` and control interpolation with blend amounts
- Creating fade effects with `fadeToBlackBy()` and fading to specific colors for trails and transitions
- Using `blur1d()` to create smooth gradients and soften sharp edges
- Cross-fading between complete patterns and layering multiple effects
- Advanced blending techniques: additive/subtractive blending, brightness scaling, and easing functions for natural motion

---

Blending and transitions are essential techniques for creating smooth, professional-looking LED animations. FastLED provides powerful functions for fading, blending, and transitioning between colors and patterns.

## Why Blending Matters

- **Professional appearance**: Smooth transitions look more polished
- **Natural motion**: Mimics how light behaves in the real world
- **Pattern continuity**: Create flowing effects without abrupt changes
- **Creative control**: Fine-tune the feel of your animations

## Color Blending

### Basic Color Blending

The `blend()` function smoothly interpolates between two colors:

```cpp
void smoothTransition() {
    static uint8_t blendAmount = 0;
    CRGB color1 = CRGB::Red;
    CRGB color2 = CRGB::Blue;

    for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = blend(color1, color2, blendAmount);
    }

    blendAmount++;  // 0-255, controls mix
}
```

**Blend Parameters:**
- `color1`: Starting color
- `color2`: Ending color
- `amount`: 0 = all color1, 255 = all color2, 128 = 50/50 mix

### Spatial Blending

Create gradients by varying blend amount across LEDs:

```cpp
void gradientBlend() {
    CRGB color1 = CRGB::Purple;
    CRGB color2 = CRGB::Cyan;

    for (int i = 0; i < NUM_LEDS; i++) {
        // Calculate blend amount for this LED
        uint8_t blendAmount = map(i, 0, NUM_LEDS-1, 0, 255);
        leds[i] = blend(color1, color2, blendAmount);
    }
}
```

### Blending Multiple Colors

```cpp
void triColorBlend() {
    CRGB color1 = CRGB::Red;
    CRGB color2 = CRGB::Green;
    CRGB color3 = CRGB::Blue;

    for (int i = 0; i < NUM_LEDS; i++) {
        if (i < NUM_LEDS / 2) {
            // Blend from color1 to color2 (first half)
            uint8_t amount = map(i, 0, NUM_LEDS/2, 0, 255);
            leds[i] = blend(color1, color2, amount);
        } else {
            // Blend from color2 to color3 (second half)
            uint8_t amount = map(i, NUM_LEDS/2, NUM_LEDS-1, 0, 255);
            leds[i] = blend(color2, color3, amount);
        }
    }
}
```

## Fade to Black

### Basic Fade to Black

Create trails and decay effects:

```cpp
void trailEffect() {
    // Fade all LEDs toward black
    fadeToBlackBy(leds, NUM_LEDS, 20);  // 20 = fade speed (0-255)

    // Add new LED
    static uint8_t pos = 0;
    leds[pos] = CRGB::White;
    pos = (pos + 1) % NUM_LEDS;
}
```

**Fade Speed Guide:**
- **5-10**: Very slow fade, long trails
- **20-40**: Moderate fade, good for most effects
- **50-100**: Fast fade, short trails
- **150-255**: Very fast fade, almost instant

### Variable Fade Rates

Different parts can fade at different speeds:

```cpp
void variableFade() {
    // Fade first half slowly
    fadeToBlackBy(leds, NUM_LEDS/2, 10);

    // Fade second half quickly
    fadeToBlackBy(&leds[NUM_LEDS/2], NUM_LEDS/2, 50);
}
```

### Fade to Specific Color

```cpp
void fadeToColor(CRGB targetColor, uint8_t fadeAmount) {
    for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = blend(leds[i], targetColor, fadeAmount);
    }
}

void loop() {
    // Gradually fade to blue
    fadeToColor(CRGB::Blue, 10);
    FastLED.show();
    delay(20);
}
```

## Blur Effects

Blur creates smooth gradients by averaging adjacent LEDs:

```cpp
void blurExample() {
    // Set some random LEDs
    leds[random8(NUM_LEDS)] = CHSV(random8(), 255, 255);

    // Blur to create smooth gradients
    blur1d(leds, NUM_LEDS, 50);  // 50 = blur amount
}
```

**Blur Amount Guide:**
- **10-30**: Subtle smoothing
- **50-100**: Moderate blur, good for most effects
- **150-200**: Heavy blur, very smooth
- **255**: Maximum blur

### Blur for Smoothing

```cpp
void smoothMovement() {
    // Create sharp point
    static uint8_t pos = 0;
    leds[pos] = CRGB::Red;

    // Blur to smooth it out
    blur1d(leds, NUM_LEDS, 100);

    // Fade everything slightly
    fadeToBlackBy(leds, NUM_LEDS, 10);

    pos = (pos + 1) % NUM_LEDS;
}
```

## Pattern Cross-Fading

Smoothly transition between two complete patterns:

```cpp
CRGB pattern1[NUM_LEDS];
CRGB pattern2[NUM_LEDS];

void crossFade(uint8_t amount) {
    for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = blend(pattern1[i], pattern2[i], amount);
    }
}

void loop() {
    static uint8_t crossFadeAmount = 0;

    // Prepare pattern 1 (rainbow)
    fill_rainbow(pattern1, NUM_LEDS, 0, 5);

    // Prepare pattern 2 (solid color)
    fill_solid(pattern2, NUM_LEDS, CRGB::Purple);

    // Cross-fade between them
    crossFade(crossFadeAmount);

    EVERY_N_MILLISECONDS(20) {
        if (crossFadeAmount < 255) {
            crossFadeAmount += 5;
        }
    }

    FastLED.show();
}
```

## Advanced Blending Techniques

### Additive Blending

Add colors together (good for light effects):

```cpp
void additiveBlend() {
    CRGB layer1 = CRGB(100, 0, 0);     // Dim red
    CRGB layer2 = CRGB(0, 100, 0);     // Dim green

    leds[0] = layer1 + layer2;         // Results in yellow (100, 100, 0)
}
```

### Subtractive Blending

Subtract colors (be careful of underflow):

```cpp
void subtractiveBlend() {
    leds[0] = CRGB(200, 100, 50);
    leds[0] -= CRGB(50, 50, 50);       // Results in (150, 50, 0)
}
```

### Scale Brightness

```cpp
void scaleBrightness() {
    leds[0] = CRGB::Red;
    leds[0].nscale8(128);              // Scale to 50% brightness
}
```

### Per-Channel Scaling

```cpp
void colorTemperature() {
    CRGB color = CRGB::White;

    // Warm white: reduce blue
    color.b = scale8(color.b, 128);
    leds[0] = color;
}
```

## Transition Timing

### Linear Transitions

```cpp
void linearTransition() {
    static uint8_t progress = 0;
    CRGB startColor = CRGB::Blue;
    CRGB endColor = CRGB::Red;

    fill_solid(leds, NUM_LEDS, blend(startColor, endColor, progress));

    progress += 2;  // Linear progression
}
```

### Eased Transitions

Add easing for more natural motion:

```cpp
uint8_t easeInOutCubic(uint8_t t) {
    if (t < 128) {
        return ((uint16_t)t * t * t) >> 14;
    } else {
        uint16_t t2 = 255 - t;
        return 255 - (((t2 * t2 * t2) >> 14));
    }
}

void easedTransition() {
    static uint8_t time = 0;
    uint8_t easedTime = easeInOutCubic(time);

    CRGB color1 = CRGB::Green;
    CRGB color2 = CRGB::Purple;

    fill_solid(leds, NUM_LEDS, blend(color1, color2, easedTime));

    time += 2;
}
```

### Wave-Based Transitions

```cpp
void waveTransition() {
    // Use sine wave for smooth back-and-forth
    uint8_t wave = beatsin8(10, 0, 255);  // 10 BPM

    CRGB color1 = CRGB::Cyan;
    CRGB color2 = CRGB::Magenta;

    fill_solid(leds, NUM_LEDS, blend(color1, color2, wave));
}
```

## Practical Examples

### Comet Trail

```cpp
void cometTrail() {
    static uint8_t pos = 0;

    // Fade all LEDs
    fadeToBlackBy(leds, NUM_LEDS, 40);

    // Bright head with blur
    leds[pos] = CRGB::White;
    blur1d(leds, NUM_LEDS, 80);

    pos = (pos + 1) % NUM_LEDS;
}
```

### Breathing Effect

```cpp
void breathe(CRGB color) {
    uint8_t brightness = beatsin8(12, 50, 255);  // Smooth oscillation

    CRGB dimmedColor = color;
    dimmedColor.nscale8(brightness);

    fill_solid(leds, NUM_LEDS, dimmedColor);
}
```

### Twinkle with Fade

```cpp
void twinkleFade() {
    // Fade all LEDs
    fadeToBlackBy(leds, NUM_LEDS, 32);

    // Add new twinkles
    if (random8() < 50) {
        int pos = random16(NUM_LEDS);
        leds[pos] = CHSV(random8(), 200, 255);
    }
}
```

### Color Chase with Blur

```cpp
void blurredChase() {
    static uint8_t hue = 0;
    static uint8_t pos = 0;

    // Fade and blur
    fadeToBlackBy(leds, NUM_LEDS, 20);
    blur1d(leds, NUM_LEDS, 60);

    // Add new position
    leds[pos] = CHSV(hue, 255, 255);

    pos = (pos + 1) % NUM_LEDS;
    hue += 3;
}
```

### Layered Effects

```cpp
void layeredEffect() {
    // Layer 1: Slow moving wave
    static uint8_t wave1 = 0;
    for (int i = 0; i < NUM_LEDS; i++) {
        uint8_t brightness = sin8(wave1 + (i * 10));
        leds[i] = CHSV(160, 255, brightness / 2);
    }
    wave1 += 2;

    // Layer 2: Fast twinkles (additive)
    if (random8() < 30) {
        int pos = random16(NUM_LEDS);
        leds[pos] += CRGB(50, 50, 50);
    }
}
```

## Blending Best Practices

1. **Start subtle**: Small fade amounts (5-20) often look best
2. **Use blur sparingly**: Too much blur can look muddy
3. **Combine techniques**: Fade + blur creates smooth, flowing effects
4. **Test timing**: Adjust speeds until motion feels natural
5. **Consider power**: Fading reduces brightness and power consumption

## Performance Tips

```cpp
// Efficient: Fade entire array at once
fadeToBlackBy(leds, NUM_LEDS, 20);

// Inefficient: Fade each LED individually
for (int i = 0; i < NUM_LEDS; i++) {
    leds[i].fadeToBlackBy(20);
}
```

## Common Pitfalls

### Too Fast Fading
```cpp
// Problem: Disappears too quickly
fadeToBlackBy(leds, NUM_LEDS, 200);

// Better: Slower fade
fadeToBlackBy(leds, NUM_LEDS, 30);
```

### Blend Amount Overflow
```cpp
// Problem: Can wrap around
uint8_t amount = 250;
amount += 10;  // Wraps to 4!

// Better: Use constrain or check
amount = min(amount + 10, 255);
```

### Forgetting to Show
```cpp
// Problem: Changes not visible
fadeToBlackBy(leds, NUM_LEDS, 20);
// Missing: FastLED.show();

// Correct:
fadeToBlackBy(leds, NUM_LEDS, 20);
FastLED.show();
```

## Complete Example

```cpp
#include <FastLED.h>

#define NUM_LEDS 60
#define DATA_PIN 6

CRGB leds[NUM_LEDS];

void setup() {
    FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(100);
}

void loop() {
    blendedChase();
    FastLED.show();
    FastLED.delay(1000 / 60);
}

void blendedChase() {
    static uint8_t pos = 0;
    static uint8_t hue = 0;

    // Fade all LEDs gradually
    fadeToBlackBy(leds, NUM_LEDS, 30);

    // Add bright LED at current position
    leds[pos] = CHSV(hue, 255, 255);

    // Blur to create smooth trail
    blur1d(leds, NUM_LEDS, 80);

    // Move to next position
    EVERY_N_MILLISECONDS(50) {
        pos = (pos + 1) % NUM_LEDS;
        hue += 4;
    }
}
```

## See Also

- [Math and Mapping Functions](math.md) - Use mathematical functions for precise control
- [Color Palettes](palettes.md) - Blend between palette colors
- [Noise and Perlin Noise](noise.md) - Use noise to control blend amounts
