# Twinkle/Sparkle Effects

**Difficulty Level**: ⭐⭐ Intermediate
**Time to Complete**: 25-35 minutes
**Prerequisites**:
- [Animations](../basic-patterns/animations.md) - fadeToBlackBy() and timing concepts
- [Color Theory](../core-concepts/color-theory.md) - HSV color space for random colors
- [Math Functions](../intermediate/math.md) - random8() and random16() functions
- [Blending](../intermediate/blending.md) - Brightness variation and fade techniques

**You'll Learn**:
- How to create probability-based animations using random8() threshold checks
- Selecting random LED positions safely with random16(NUM_LEDS)
- Tuning fade rates and probability thresholds to control twinkle density and persistence
- Creating variations (starfield, party lights, fireflies) by adjusting parameters
- Using brightness variation and timing to add depth and dimension to sparkle effects

Random twinkling stars and sparkle effects.

## Simple Twinkle

Basic twinkling effect with random colors:

```cpp
void twinkle() {
    // Fade all LEDs slightly
    fadeToBlackBy(leds, NUM_LEDS, 32);

    // Add new twinkles
    if (random8() < 50) {  // 50/255 chance per frame
        int pos = random16(NUM_LEDS);
        leds[pos] = CHSV(random8(), 200, 255);  // Random color
    }
}
```

## Colored Twinkle

Twinkle effect with a specific color:

```cpp
void coloredTwinkle(CRGB color) {
    fadeToBlackBy(leds, NUM_LEDS, 20);

    // Add multiple twinkles per frame
    for (int i = 0; i < 3; i++) {
        if (random8() < 100) {
            int pos = random16(NUM_LEDS);
            leds[pos] = color;
            leds[pos].fadeToBlackBy(random8(50, 150));  // Vary brightness
        }
    }
}
```

## Sparkle (Instant Flash)

Quick flashing sparkles on a dark background:

```cpp
void sparkle(CRGB color, int count, int delayMs) {
    // Turn off all LEDs
    FastLED.clear();

    // Flash random LEDs
    for (int i = 0; i < count; i++) {
        int pos = random16(NUM_LEDS);
        leds[pos] = color;
    }

    FastLED.show();
    delay(delayMs);
}
```

## White Twinkle (Christmas Lights)

Warm white background with bright white twinkles:

```cpp
void whiteTwinkle() {
    // Background: warm white
    for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = CRGB(255, 147, 41);
        leds[i].fadeToBlackBy(200);  // Dim background
    }

    // Random bright twinkles
    for (int i = 0; i < 5; i++) {
        if (random8() < 80) {
            int pos = random16(NUM_LEDS);
            leds[pos] = CRGB::White;
        }
    }

    FastLED.show();
    delay(100);
}
```

## How It Works

The twinkle effect creates a starry night appearance by:

1. **Fading**: Gradually dimming all LEDs to create trails
2. **Random selection**: Choosing random LEDs to light up
3. **Brightness variation**: Using different brightness levels for depth
4. **Color variation**: Optional random or specific colors

## Parameters Explained

- **Fade rate** (in fadeToBlackBy): How quickly LEDs dim
  - Lower values (10-20) = long, smooth trails
  - Higher values (50+) = quick, snappy twinkles

- **Probability** (in random8() check): How often new twinkles appear
  - Lower values (20-50) = sparse, occasional twinkles
  - Higher values (100-200) = frequent, dense twinkles

- **Count**: Number of simultaneous sparkles
  - 1-3 for subtle effect
  - 5-10 for busy starfield

## Usage Tips

**For a starfield effect:**
```cpp
void loop() {
    fadeToBlackBy(leds, NUM_LEDS, 32);
    if (random8() < 30) {
        leds[random16(NUM_LEDS)] = CRGB::White;
    }
    FastLED.show();
    delay(20);
}
```

**For party lights:**
```cpp
void loop() {
    fadeToBlackBy(leds, NUM_LEDS, 50);
    for (int i = 0; i < 5; i++) {
        if (random8() < 100) {
            leds[random16(NUM_LEDS)] = CHSV(random8(), 255, 255);
        }
    }
    FastLED.show();
    delay(50);
}
```

**For fireflies:**
```cpp
void loop() {
    fadeToBlackBy(leds, NUM_LEDS, 10);  // Very slow fade
    if (random8() < 20) {
        leds[random16(NUM_LEDS)] = CRGB(255, 255, 100);  // Yellowish
    }
    FastLED.show();
    delay(30);
}
```

## Variations

**Palette-Based Twinkle:**
```cpp
void paletteTwinkle(CRGBPalette16 palette) {
    fadeToBlackBy(leds, NUM_LEDS, 40);
    if (random8() < 60) {
        int pos = random16(NUM_LEDS);
        leds[pos] = ColorFromPalette(palette, random8(), 255, LINEARBLEND);
    }
}
```

**Twinkling Rainbow:**
```cpp
void rainbowTwinkle() {
    fadeToBlackBy(leds, NUM_LEDS, 30);
    if (random8() < 80) {
        int pos = random16(NUM_LEDS);
        leds[pos] = CHSV(random8(), 255, 255);
    }
}
```
