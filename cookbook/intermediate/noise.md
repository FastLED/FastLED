# Noise and Perlin Noise

**Difficulty Level**: ⭐⭐ Intermediate
**Time to Complete**: 45-55 minutes
**Prerequisites**:
- [Color Palettes](palettes.md) - Noise is often combined with palettes for stunning effects
- [Color Theory](../core-concepts/color-theory.md) - Understanding HSV and RGB color models
- [Animations](../basic-patterns/animations.md) - Static variables and timing concepts

**You'll Learn**:
- What Perlin noise is and how it differs from random values for creating smooth, organic animations
- Using FastLED's noise functions (`inoise8`, `inoise16`) in 1D, 2D, and 3D for different effect complexity
- Controlling noise parameters: spatial coordinates (smoothness), time component (animation speed), and Z dimension (depth)
- Combining noise with color palettes to create natural effects like lava lamps, clouds, ocean waves, and fire
- Advanced techniques: layered noise for richer patterns, noise-controlled brightness, and noise-based movement

---

FastLED includes noise functions for creating organic, natural-looking animations. Unlike random values, noise produces smooth, continuous patterns that flow naturally over time and space.

## What is Perlin Noise?

Perlin noise is a type of gradient noise that creates smooth, natural-looking randomness. It's commonly used for:
- Lava lamp effects
- Cloud movement
- Fire simulation
- Organic flowing patterns
- Terrain generation

Unlike `random()` which produces abrupt changes, noise creates smooth transitions between values.

## Basic Noise Functions

FastLED provides several noise functions:

```cpp
// 1D noise (single parameter)
uint8_t inoise8(uint16_t x);

// 2D noise (two parameters)
uint8_t inoise8(uint16_t x, uint16_t y);

// 3D noise (three parameters)
uint8_t inoise8(uint16_t x, uint16_t y, uint16_t z);

// 16-bit noise for higher precision
uint16_t inoise16(uint32_t x, uint32_t y, uint32_t z);
```

All noise functions return values in the range 0-255 (for `inoise8`) or 0-65535 (for `inoise16`).

## Basic Noise Example

```cpp
void noiseEffect() {
    static uint16_t x = 0;

    for (int i = 0; i < NUM_LEDS; i++) {
        // Generate noise value (0-255)
        uint8_t noise = inoise8(x + (i * 100), millis() / 10);

        // Map to color (hue)
        leds[i] = CHSV(noise, 255, 255);
    }

    x += 10;  // Move through noise space
}
```

## Understanding Noise Parameters

The key to using noise effectively is understanding how the parameters affect the output:

### Spatial Coordinates

```cpp
// Each LED gets a different position in noise space
for (int i = 0; i < NUM_LEDS; i++) {
    uint8_t noise = inoise8(i * 50);  // Spacing controls smoothness
    leds[i] = CHSV(noise, 255, 255);
}
```

- **Small spacing** (10-50): Very smooth transitions
- **Medium spacing** (50-100): Balanced, natural look
- **Large spacing** (100-300): More chaotic, faster changes

### Time Component

```cpp
void animatedNoise() {
    for (int i = 0; i < NUM_LEDS; i++) {
        // Add time-based animation
        uint8_t noise = inoise8(
            i * 50,            // Spatial position
            millis() / 20      // Time component
        );
        leds[i] = CHSV(noise, 255, 255);
    }
}
```

The divisor on `millis()` controls animation speed:
- **/100**: Slow, gentle movement
- **/50**: Moderate speed
- **/20**: Fast movement
- **/10**: Very fast

## 2D Noise for Movement

Using two dimensions creates more complex, organic patterns:

```cpp
void lavaLamp() {
    for (int i = 0; i < NUM_LEDS; i++) {
        // Create time-varying noise
        uint8_t noise = inoise8(
            i * 50,                    // X position (spatial)
            millis() / 20 + i * 10     // Y position (time-based)
        );

        // Use with heat colors palette
        leds[i] = ColorFromPalette(HeatColors_p, noise);
    }
}
```

## 3D Noise

Three dimensions provide the most complex and interesting patterns:

```cpp
void complexNoise() {
    static uint16_t z = 0;

    for (int i = 0; i < NUM_LEDS; i++) {
        uint8_t noise = inoise8(
            i * 50,           // X: spatial position
            millis() / 20,    // Y: time
            z                 // Z: additional animation dimension
        );

        leds[i] = CHSV(noise, 255, 255);
    }

    z += 1;  // Slowly move through Z dimension
}
```

## Noise with Color Palettes

Combining noise with palettes creates beautiful effects:

```cpp
void noisePalette() {
    CRGBPalette16 palette = OceanColors_p;

    for (int i = 0; i < NUM_LEDS; i++) {
        uint8_t noise = inoise8(i * 60, millis() / 30);
        leds[i] = ColorFromPalette(palette, noise, 255, LINEARBLEND);
    }
}
```

## Layered Noise

Combine multiple noise layers for richer effects:

```cpp
void layeredNoise() {
    for (int i = 0; i < NUM_LEDS; i++) {
        // Two noise layers with different scales
        uint8_t noise1 = inoise8(i * 30, millis() / 20);
        uint8_t noise2 = inoise8(i * 80, millis() / 50);

        // Combine layers
        uint8_t combined = (noise1 + noise2) / 2;

        leds[i] = CHSV(combined, 255, 255);
    }
}
```

## Practical Examples

### Cloud Effect

```cpp
void clouds() {
    CRGBPalette16 cloudPalette = CloudColors_p;

    for (int i = 0; i < NUM_LEDS; i++) {
        uint8_t noise = inoise8(
            i * 40,
            millis() / 50  // Slow drift
        );

        leds[i] = ColorFromPalette(cloudPalette, noise, 255, LINEARBLEND);
    }
}
```

### Lava Effect

```cpp
void lava() {
    CRGBPalette16 lavaPalette = LavaColors_p;

    for (int i = 0; i < NUM_LEDS; i++) {
        uint8_t noise = inoise8(
            i * 60,
            millis() / 25 + i * 15
        );

        leds[i] = ColorFromPalette(lavaPalette, noise, 255, LINEARBLEND);
    }
}
```

### Ocean Waves

```cpp
void oceanWaves() {
    CRGBPalette16 oceanPalette = OceanColors_p;
    static uint16_t z = 0;

    for (int i = 0; i < NUM_LEDS; i++) {
        // Three layers for depth
        uint8_t noise1 = inoise8(i * 40, millis() / 30, z);
        uint8_t noise2 = inoise8(i * 70, millis() / 40, z + 1000);
        uint8_t combined = (noise1 + noise2) / 2;

        leds[i] = ColorFromPalette(oceanPalette, combined, 255, LINEARBLEND);
    }

    z += 2;
}
```

### Rainbow Noise

```cpp
void rainbowNoise() {
    static uint16_t offset = 0;

    for (int i = 0; i < NUM_LEDS; i++) {
        uint8_t noise = inoise8(i * 50 + offset, millis() / 20);
        leds[i] = CHSV(noise, 200, 255);
    }

    offset += 5;
}
```

## Noise with Brightness

Use noise to control brightness instead of color:

```cpp
void noiseBrightness() {
    CRGB baseColor = CRGB::Blue;

    for (int i = 0; i < NUM_LEDS; i++) {
        uint8_t brightness = inoise8(i * 60, millis() / 30);

        leds[i] = baseColor;
        leds[i].nscale8(brightness);
    }
}
```

## Performance Considerations

Noise functions are computationally efficient, but here are some tips:

1. **Reuse values**: If multiple LEDs use the same noise, calculate once
2. **Adjust spacing**: Larger increments = less smooth but faster
3. **Limit dimensions**: 2D noise is faster than 3D
4. **Consider frame rate**: Target 30-60 FPS for smooth animation

## Noise Parameters Guide

### X Parameter (Spatial)
- Controls position along strip
- Multiply LED index by 20-100
- Larger = more variation between adjacent LEDs

### Y Parameter (Time)
- Usually `millis() / divisor`
- Divisor 10-100 controls speed
- Smaller divisor = faster animation

### Z Parameter (Extra dimension)
- Adds additional animation layer
- Increment slowly (1-5 per frame)
- Creates "depth" to animation

## Common Issues

### Too Chaotic
```cpp
// Problem: Too much variation
uint8_t noise = inoise8(i * 200, millis() / 5);

// Solution: Reduce spacing and slow down
uint8_t noise = inoise8(i * 50, millis() / 30);
```

### Too Static
```cpp
// Problem: Barely moving
uint8_t noise = inoise8(i * 10, millis() / 200);

// Solution: Increase spacing or speed up
uint8_t noise = inoise8(i * 50, millis() / 30);
```

### Too Uniform
```cpp
// Problem: All LEDs same color
uint8_t noise = inoise8(i, millis() / 30);

// Solution: Increase spatial multiplier
uint8_t noise = inoise8(i * 50, millis() / 30);
```

## Advanced: Noise-Based Movement

Create moving patterns using noise to control position:

```cpp
void noisyMovement() {
    fadeToBlackBy(leds, NUM_LEDS, 50);

    // Use noise to determine which LED to light
    uint8_t pos = inoise8(millis() / 10) % NUM_LEDS;
    leds[pos] = CHSV(160, 255, 255);
}
```

## Complete Example

```cpp
#include <FastLED.h>

#define NUM_LEDS 60
#define DATA_PIN 6

CRGB leds[NUM_LEDS];
CRGBPalette16 currentPalette = OceanColors_p;

void setup() {
    FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(100);
}

void loop() {
    noiseEffect();
    FastLED.show();
    FastLED.delay(1000 / 60);  // 60 FPS
}

void noiseEffect() {
    static uint16_t z = 0;

    for (int i = 0; i < NUM_LEDS; i++) {
        // Generate smooth noise
        uint8_t noise = inoise8(
            i * 50,           // Spatial spacing
            millis() / 30,    // Time-based animation
            z                 // Extra dimension
        );

        // Map to palette
        leds[i] = ColorFromPalette(currentPalette, noise, 255, LINEARBLEND);
    }

    z += 2;  // Move through Z dimension
}
```

## See Also

- [Color Palettes](palettes.md) - Use palettes with noise for stunning effects
- [Math and Mapping Functions](math.md) - Combine noise with mathematical functions
