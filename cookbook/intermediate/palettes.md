# Color Palettes

**Difficulty Level**: ⭐⭐ Intermediate
**Time to Complete**: 40-50 minutes
**Prerequisites**:
- [Basic Patterns](../basic-patterns/README.md) - Understanding fill functions and animations
- [Color Theory](../core-concepts/color-theory.md) - RGB and HSV color models
- [Animations](../basic-patterns/animations.md) - Static variables and timing

**You'll Learn**:
- How to use FastLED's built-in color palettes for instant professional color schemes
- How to create custom palettes with gradient definitions and direct color specification
- How to retrieve and blend colors from palettes using `ColorFromPalette()`
- Palette animation techniques: moving, stretching, pulsing, and transitioning between palettes
- Advanced techniques combining palettes with noise, multiple palettes, and smooth palette blending with `nblendPaletteTowardPalette()`

---

Color palettes allow you to define a set of colors and smoothly blend between them. This is one of the most powerful features in FastLED for creating sophisticated color effects.

## Why Use Palettes?

- **Smooth transitions**: Automatically blend between colors
- **Easy to change**: Swap entire color schemes by changing one line
- **Professional look**: Create cohesive color schemes
- **Memory efficient**: Store complex gradients compactly

## Predefined Palettes

FastLED comes with several built-in color palettes ready to use:

```cpp
#include <FastLED.h>

CRGBPalette16 palette = RainbowColors_p;  // Built-in rainbow palette

// Other built-in palettes:
// - PartyColors_p
// - CloudColors_p
// - LavaColors_p
// - OceanColors_p
// - ForestColors_p
// - HeatColors_p
```

### Built-in Palette Descriptions

- **RainbowColors_p**: Full spectrum rainbow
- **RainbowStripeColors_p**: Rainbow with black stripes
- **PartyColors_p**: Vibrant party colors (purple, orange, blue)
- **CloudColors_p**: Soft blue and white for cloud effects
- **LavaColors_p**: Red, orange, and yellow for fire effects
- **OceanColors_p**: Blue and aqua for water effects
- **ForestColors_p**: Green tones for nature effects
- **HeatColors_p**: Black to red to yellow to white (heat map)

## Custom Palettes

Create your own color palettes to match your specific needs:

```cpp
// Define colors at specific positions (0-255)
DEFINE_GRADIENT_PALETTE(sunset_gp) {
    0,   255, 0,   0,    // Red at position 0
    128, 255, 128, 0,    // Orange at position 128
    255, 255, 255, 0     // Yellow at position 255
};

CRGBPalette16 sunsetPalette = sunset_gp;
```

### Custom Palette with Multiple Colors

```cpp
DEFINE_GRADIENT_PALETTE(ocean_gp) {
    0,     0,   0,  128,   // Deep blue at start
    64,    0,  64,  255,   // Medium blue
    128,   0, 128,  255,   // Cyan
    192,   0, 255,  192,   // Light cyan
    255,  64, 255,  255    // White foam at end
};

CRGBPalette16 oceanPalette = ocean_gp;
```

### Direct Color Definition

```cpp
// Create palette from specific CRGB colors
CRGBPalette16 myPalette = CRGBPalette16(
    CRGB::Red,
    CRGB::Orange,
    CRGB::Yellow,
    CRGB::Green,
    CRGB::Blue,
    CRGB::Purple,
    CRGB::Pink,
    CRGB::White
);
```

## Using Palettes

The `ColorFromPalette()` function retrieves colors from a palette with automatic blending:

```cpp
void paletteEffect() {
    static uint8_t paletteIndex = 0;

    for (int i = 0; i < NUM_LEDS; i++) {
        // Calculate color index for this LED
        uint8_t colorIndex = paletteIndex + (i * 4);

        // Get color from palette with linear blending
        leds[i] = ColorFromPalette(palette, colorIndex, 255, LINEARBLEND);
    }

    paletteIndex++;  // Animate through palette
}
```

### ColorFromPalette Parameters

```cpp
ColorFromPalette(
    palette,      // The palette to use
    index,        // Color index (0-255)
    brightness,   // Brightness (0-255)
    blendType     // LINEARBLEND or NOBLEND
);
```

- **palette**: Your CRGBPalette16 object
- **index**: Position in palette (0-255, wraps around)
- **brightness**: Overall brightness multiplier
- **blendType**:
  - `LINEARBLEND`: Smooth gradients (recommended)
  - `NOBLEND`: Discrete color steps

## Palette Animation Techniques

### Moving Through Palette

```cpp
void movingPalette() {
    static uint8_t startIndex = 0;

    startIndex++;  // Move through palette over time

    for (int i = 0; i < NUM_LEDS; i++) {
        uint8_t colorIndex = startIndex + (i * 2);
        leds[i] = ColorFromPalette(palette, colorIndex, 255, LINEARBLEND);
    }
}
```

### Stretching the Palette

```cpp
void stretchedPalette() {
    for (int i = 0; i < NUM_LEDS; i++) {
        // Map LED position to full palette range
        uint8_t colorIndex = map(i, 0, NUM_LEDS-1, 0, 255);
        leds[i] = ColorFromPalette(palette, colorIndex, 255, LINEARBLEND);
    }
}
```

### Dynamic Brightness

```cpp
void palettePulse() {
    static uint8_t paletteIndex = 0;
    uint8_t brightness = beatsin8(15, 50, 255);  // Pulsing brightness

    for (int i = 0; i < NUM_LEDS; i++) {
        uint8_t colorIndex = paletteIndex + (i * 3);
        leds[i] = ColorFromPalette(palette, colorIndex, brightness, LINEARBLEND);
    }

    paletteIndex++;
}
```

## Palette Blending

Smoothly transition between different palettes:

```cpp
CRGBPalette16 currentPalette;
CRGBPalette16 targetPalette;

void setup() {
    currentPalette = RainbowColors_p;
    targetPalette = LavaColors_p;
}

void loop() {
    // Blend toward target palette
    EVERY_N_MILLISECONDS(10) {
        nblendPaletteTowardPalette(currentPalette, targetPalette, 12);
    }

    // Change target palette periodically
    EVERY_N_SECONDS(5) {
        targetPalette = targetPalette == RainbowColors_p ?
                        LavaColors_p : RainbowColors_p;
    }

    // Use current palette
    for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = ColorFromPalette(currentPalette, i * 4, 255, LINEARBLEND);
    }

    FastLED.show();
}
```

## Advanced Palette Techniques

### Noise-Based Palette Selection

```cpp
void noisyPalette() {
    for (int i = 0; i < NUM_LEDS; i++) {
        // Use noise to select palette color
        uint8_t noise = inoise8(i * 50, millis() / 10);
        leds[i] = ColorFromPalette(palette, noise, 255, LINEARBLEND);
    }
}
```

### Multiple Palettes

```cpp
CRGBPalette16 palette1 = RainbowColors_p;
CRGBPalette16 palette2 = LavaColors_p;

void dualPalette() {
    for (int i = 0; i < NUM_LEDS; i++) {
        // Alternate between palettes
        CRGBPalette16 &usePalette = (i % 2) ? palette1 : palette2;
        leds[i] = ColorFromPalette(usePalette, i * 8, 255, LINEARBLEND);
    }
}
```

## Palette Best Practices

1. **Use LINEARBLEND**: Almost always use `LINEARBLEND` for smoother effects
2. **Index spacing**: Multiply LED index by 2-8 for good color distribution
3. **Animation speed**: Increment index by 1-4 per frame for smooth motion
4. **Brightness**: Use full brightness (255) unless pulsing is desired
5. **Palette transitions**: Use `nblendPaletteTowardPalette()` for smooth changes

## Example: Complete Palette Effect

```cpp
#include <FastLED.h>

#define NUM_LEDS 60
#define DATA_PIN 6

CRGB leds[NUM_LEDS];
CRGBPalette16 currentPalette;
CRGBPalette16 targetPalette;
TBlendType currentBlending = LINEARBLEND;

void setup() {
    FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(50);

    currentPalette = RainbowColors_p;
    targetPalette = RainbowColors_p;
}

void loop() {
    // Change palette every 5 seconds
    EVERY_N_SECONDS(5) {
        ChangePalettePeriodically();
    }

    // Blend toward target palette
    EVERY_N_MILLISECONDS(10) {
        nblendPaletteTowardPalette(currentPalette, targetPalette, 12);
    }

    // Animate palette
    static uint8_t startIndex = 0;
    startIndex++;

    FillLEDsFromPaletteColors(startIndex);

    FastLED.show();
    FastLED.delay(1000 / 60);  // 60 FPS
}

void FillLEDsFromPaletteColors(uint8_t colorIndex) {
    uint8_t brightness = 255;

    for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = ColorFromPalette(currentPalette, colorIndex, brightness, currentBlending);
        colorIndex += 3;
    }
}

void ChangePalettePeriodically() {
    uint8_t secondHand = (millis() / 1000) % 60;
    static uint8_t lastSecond = 99;

    if (lastSecond != secondHand) {
        lastSecond = secondHand;
        if (secondHand == 0) { targetPalette = RainbowColors_p; }
        if (secondHand == 10) { targetPalette = LavaColors_p; }
        if (secondHand == 20) { targetPalette = OceanColors_p; }
        if (secondHand == 30) { targetPalette = ForestColors_p; }
        if (secondHand == 40) { targetPalette = PartyColors_p; }
        if (secondHand == 50) { targetPalette = CloudColors_p; }
    }
}
```

## See Also

- [Noise and Perlin Noise](noise.md) - Use palettes with noise for organic effects
- [Blending and Transitions](blending.md) - Combine palettes with blending techniques
