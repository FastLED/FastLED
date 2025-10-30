# Wave Patterns

**Difficulty Level**: ⭐⭐ Intermediate
**Time to Complete**: 35-45 minutes
**Prerequisites**:
- [Basic Patterns: Animations](../basic-patterns/animations.md) - Understanding timing and static variables
- [Core Concepts: Color Theory](../core-concepts/color-theory.md) - RGB and HSV color spaces
- [Intermediate: Math](../intermediate/math.md) - sin8(), beatsin8(), and trigonometric wave functions (core dependency)
- [Intermediate: Blending](../intermediate/blending.md) - fadeToBlackBy() for trail effects

**You'll Learn**:
- How to create traveling wave animations using sin8() trigonometric function with phase and position parameters
- Controlling wave characteristics: speed via phase increment, frequency via position multiplier, wavelength via spread
- Layering multiple waves together to create complex organic patterns (multi-wave, plasma, ocean simulation)
- Creating directional wave effects: ripples expanding from center point, dual waves with phase offsets, traveling peaks
- Applying wave techniques to practical patterns: color waves moving along strip, smooth scanners with sine falloff, ocean waves (Pacifica), pride effect with multiple modulated parameters

Various wave effects including sine waves, color waves, and ocean-inspired patterns.

## Sine Wave

Basic sine wave pattern moving along the strip:

```cpp
void sineWave() {
    static uint8_t phase = 0;

    for (int i = 0; i < NUM_LEDS; i++) {
        // Calculate sine wave brightness for each LED
        uint8_t brightness = sin8(phase + (i * 255 / NUM_LEDS));
        leds[i] = CHSV(160, 255, brightness);
    }

    phase += 4;  // Wave speed
}
```

## Color Wave

Wave of changing colors traveling along the strip:

```cpp
void colorWave() {
    static uint8_t hue = 0;

    for (int i = 0; i < NUM_LEDS; i++) {
        // Each LED gets a different hue based on position
        uint8_t ledHue = hue + (i * 10);
        leds[i] = CHSV(ledHue, 255, 255);
    }

    hue++;  // Shift colors
}
```

## Multiple Waves

Combine three waves with different speeds and phases:

```cpp
void multiWave() {
    for (int i = 0; i < NUM_LEDS; i++) {
        // Combine three waves with different speeds and phases
        uint8_t wave1 = sin8(millis() / 10 + i * 8);
        uint8_t wave2 = sin8(millis() / 20 + i * 12);
        uint8_t wave3 = sin8(millis() / 30 + i * 16);

        uint8_t combined = (wave1 + wave2 + wave3) / 3;
        leds[i] = CHSV(combined, 255, 255);
    }
}
```

## Pacifica (Ocean Waves)

Complex ocean wave simulation with multiple layers:

```cpp
void pacifica() {
    // Four layers of waves
    pacifica_one_layer(1, 26, 100, 11);
    pacifica_one_layer(2, 13, 65, 17);
    pacifica_one_layer(3, 8, 43, 23);
    pacifica_one_layer(4, 5, 28, 29);

    // Add whitecaps
    pacifica_add_whitecaps();

    // Deepen colors
    pacifica_deepen_colors();
}

void pacifica_one_layer(uint8_t layer, uint16_t speed, uint8_t scale, uint8_t offset) {
    uint16_t dataOffset = layer * NUM_LEDS;
    for (int i = 0; i < NUM_LEDS; i++) {
        uint16_t angle = (millis() / speed) + (i * scale) + offset;
        uint8_t level = sin8(angle) / 2 + 127;
        leds[i] += CHSV(200, 255, level);  // Blue-green
    }
}

void pacifica_add_whitecaps() {
    uint8_t basethreshold = beatsin8(9, 55, 65);
    for (int i = 0; i < NUM_LEDS; i++) {
        uint8_t threshold = basethreshold + (i * 2);
        if (leds[i].blue > threshold) {
            uint8_t overage = leds[i].blue - threshold;
            leds[i] += CRGB(overage, overage, overage);
        }
    }
}

void pacifica_deepen_colors() {
    for (int i = 0; i < NUM_LEDS; i++) {
        leds[i].blue = scale8(leds[i].blue, 145);
        leds[i].green = scale8(leds[i].green, 200);
    }
}
```

## Ripple Effect

Expanding ripples from a center point:

```cpp
int center = NUM_LEDS / 2;
uint8_t maxDistance = NUM_LEDS / 2;

void ripple() {
    static uint8_t wavePos = 0;

    fadeToBlackBy(leds, NUM_LEDS, 10);

    for (int i = 0; i < NUM_LEDS; i++) {
        // Distance from center
        uint8_t distance = abs(i - center);

        // Create ripple
        if (distance == wavePos || distance == wavePos - 5) {
            leds[i] = CHSV(160, 255, 255);
        }
    }

    wavePos++;
    if (wavePos >= maxDistance) wavePos = 0;
}
```

## How It Works

Wave patterns use trigonometric functions to create smooth, flowing motion:

1. **sin8()**: 8-bit sine function (fast and efficient)
2. **Phase**: Controls position in the wave cycle
3. **Frequency**: Determined by the multiplier applied to position
4. **Speed**: Controlled by incrementing the phase value

## Parameters Explained

**Sine Wave Components:**
- **phase**: Current position in animation (increment for movement)
- **i * multiplier**: Spreads wave across strip (higher = more waves)
- **phase increment**: Controls speed (higher = faster movement)

**Wave Frequency:**
- Small multiplier (2-5): Gentle, long wavelengths
- Medium multiplier (8-15): Visible wave patterns
- Large multiplier (20+): Rapid, compressed waves

## Usage Examples

**Smooth scanner:**
```cpp
void loop() {
    static uint8_t pos = 0;

    for (int i = 0; i < NUM_LEDS; i++) {
        int distance = abs(i - pos);
        uint8_t brightness = qadd8(sin8((255 / NUM_LEDS) * distance), 50);
        leds[i] = CHSV(0, 255, brightness);
    }

    pos++;
    if (pos >= NUM_LEDS) pos = 0;

    FastLED.show();
    delay(20);
}
```

**Pride wave:**
```cpp
void pride() {
    static uint16_t sPseudotime = 0;
    static uint16_t sLastMillis = 0;
    static uint16_t sHue16 = 0;

    uint8_t sat8 = beatsin88(87, 220, 250);
    uint8_t brightdepth = beatsin88(341, 96, 224);
    uint16_t brightnessthetainc16 = beatsin88(203, (25 * 256), (40 * 256));
    uint8_t msmultiplier = beatsin88(147, 23, 60);

    uint16_t hue16 = sHue16;
    uint16_t hueinc16 = beatsin88(113, 1, 3000);

    uint16_t ms = millis();
    uint16_t deltams = ms - sLastMillis;
    sLastMillis = ms;
    sPseudotime += deltams * msmultiplier;
    sHue16 += deltams * beatsin88(400, 5, 9);
    uint16_t brightnesstheta16 = sPseudotime;

    for (uint16_t i = 0; i < NUM_LEDS; i++) {
        hue16 += hueinc16;
        uint8_t hue8 = hue16 / 256;

        brightnesstheta16 += brightnessthetainc16;
        uint16_t b16 = sin16(brightnesstheta16) + 32768;

        uint16_t bri16 = (uint32_t)((uint32_t)b16 * (uint32_t)b16) / 65536;
        uint8_t bri8 = (uint32_t)(((uint32_t)bri16) * brightdepth) / 65536;
        bri8 += (255 - brightdepth);

        CRGB newcolor = CHSV(hue8, sat8, bri8);

        uint16_t pixelnumber = i;
        pixelnumber = (NUM_LEDS - 1) - pixelnumber;

        nblend(leds[pixelnumber], newcolor, 64);
    }
}
```

## Variations

**Dual Wave:**
```cpp
void dualWave() {
    static uint8_t phase = 0;

    for (int i = 0; i < NUM_LEDS; i++) {
        uint8_t wave1 = sin8(phase + (i * 8));
        uint8_t wave2 = sin8(phase + (i * 16) + 128);  // Offset phase

        leds[i] = CHSV(wave1, 255, wave2);
    }

    phase += 2;
}
```

**Plasma Wave:**
```cpp
void plasma() {
    static uint8_t time_offset = 0;

    for (int i = 0; i < NUM_LEDS; i++) {
        uint8_t val1 = sin8(time_offset + i * 8);
        uint8_t val2 = sin8(time_offset * 2 + i * 4);
        uint8_t val3 = sin8(time_offset / 2 + i * 16);

        uint8_t combined = (val1 + val2 + val3) / 3;
        leds[i] = CHSV(combined, 255, 255);
    }

    time_offset++;
}
```

**Traveling Peaks:**
```cpp
void travelingPeaks() {
    static uint8_t peak1 = 0;
    static uint8_t peak2 = NUM_LEDS / 2;

    fadeToBlackBy(leds, NUM_LEDS, 20);

    // Draw peaks with falloff
    for (int i = 0; i < NUM_LEDS; i++) {
        int dist1 = abs(i - peak1);
        int dist2 = abs(i - peak2);

        uint8_t brightness = qadd8(
            qsub8(255, dist1 * 20),
            qsub8(255, dist2 * 20)
        );

        leds[i] += CHSV(160, 255, brightness);
    }

    peak1 = (peak1 + 1) % NUM_LEDS;
    peak2 = (peak2 + 1) % NUM_LEDS;
}
```

## Tips

- Use `sin8()` instead of `sin()` for better performance
- Combine multiple waves for complex patterns
- Adjust phase increment for different speeds
- Use `fadeToBlackBy()` with waves for trail effects
- Experiment with different color mappings (hue, saturation, or brightness)
- Layer waves by adding their results together
- Use `millis()` for time-based animation that doesn't depend on frame rate
