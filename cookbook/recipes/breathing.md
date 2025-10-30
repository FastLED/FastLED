# Breathing Effect

**Difficulty Level**: ⭐⭐ Intermediate
**Time to Complete**: 30-40 minutes
**Prerequisites**:
- [Basic Patterns: Animations](../basic-patterns/animations.md) - Understanding timing and static variables
- [Core Concepts: Color Theory](../core-concepts/color-theory.md) - RGB and HSV color spaces
- [Intermediate: Math](../intermediate/math.md) - beatsin8() and wave functions
- [Intermediate: Blending](../intermediate/blending.md) - fadeToBlackBy() for brightness control

**You'll Learn**:
- How to create smooth pulsing/breathing animations using beatsin8() sine wave function
- Controlling breathing speed with BPM (breaths per minute) parameter and depth with min/max brightness range
- Creating phased breathing effects where LEDs pulse in sequence using phase offset parameter
- Combining multiple breathing patterns (dual colors, rainbow breathing, sectioned breathing with different phases)
- Applying breathing to practical use cases: meditation lights, alert indicators, ocean waves, heartbeat patterns

Smooth pulsing/breathing animations.

## Simple Breathing

Basic breathing effect with a solid color:

```cpp
void breathe(CRGB color) {
    // Use beatsin8 for smooth sine wave (0-255)
    uint8_t brightness = beatsin8(12);  // 12 BPM

    // Set all LEDs to color with varying brightness
    for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = color;
        leds[i].fadeToBlackBy(255 - brightness);
    }
}
```

## Multi-Color Breathing

Breathing effect with slowly changing colors:

```cpp
void colorBreathe() {
    static uint8_t hue = 0;

    // Brightness oscillates
    uint8_t brightness = beatsin8(10, 50, 255);

    // Hue slowly changes
    EVERY_N_MILLISECONDS(50) {
        hue++;
    }

    fill_solid(leds, NUM_LEDS, CHSV(hue, 255, brightness));
}
```

## Phased Breathing

LEDs breathe in sequence, creating a wave effect:

```cpp
void phasedBreathe() {
    for (int i = 0; i < NUM_LEDS; i++) {
        // Each LED has a phase offset
        uint8_t brightness = beatsin8(
            15,                      // Speed
            50,                      // Minimum brightness
            255,                     // Maximum brightness
            0,                       // Time offset
            i * (255 / NUM_LEDS)    // Phase per LED
        );

        leds[i] = CHSV(160, 255, brightness);
    }
}
```

## Dual Breathing

Two colors alternating, breathing in opposite phases:

```cpp
void dualBreathe() {
    uint8_t brightness1 = beatsin8(8);
    uint8_t brightness2 = 255 - brightness1;  // Inverse

    for (int i = 0; i < NUM_LEDS; i++) {
        CRGB color1 = CRGB::Blue;
        CRGB color2 = CRGB::Purple;

        color1.fadeToBlackBy(255 - brightness1);
        color2.fadeToBlackBy(255 - brightness2);

        leds[i] = color1 + color2;  // Blend both colors
    }
}
```

## How It Works

The breathing effect uses sine wave functions to create smooth oscillations:

1. **beatsin8()**: Generates smooth sine wave values (0-255)
2. **BPM parameter**: Controls breathing speed (breaths per minute)
3. **Min/Max values**: Set breathing depth range
4. **Phase offset**: Creates wave patterns when different per LED

## Parameters Explained

**beatsin8(bpm, min, max, timebase, phase_offset)**

- **bpm** (beats per minute): Controls breathing speed
  - 5-10 = slow, relaxing breathing
  - 12-15 = normal breathing pace
  - 20+ = rapid pulsing

- **min/max**: Brightness range
  - (0, 255) = full range, complete off to full bright
  - (50, 255) = never completely dark
  - (100, 200) = subtle pulsing

- **phase_offset**: Staggers timing across LEDs
  - 0 = all LEDs breathe together
  - `i * (255 / NUM_LEDS)` = smooth wave across strip

## Usage Examples

**Relaxing meditation lights:**
```cpp
void loop() {
    uint8_t brightness = beatsin8(6, 30, 200);  // Slow, never too dim
    fill_solid(leds, NUM_LEDS, CHSV(160, 180, brightness));  // Soft blue
    FastLED.show();
}
```

**Alert indicator:**
```cpp
void loop() {
    uint8_t brightness = beatsin8(30);  // Fast breathing
    fill_solid(leds, NUM_LEDS, CRGB::Red);
    FastLED.setBrightness(brightness);
    FastLED.show();
}
```

**Ocean waves:**
```cpp
void loop() {
    for (int i = 0; i < NUM_LEDS; i++) {
        uint8_t brightness = beatsin8(8, 50, 255, 0, i * 4);
        leds[i] = CHSV(160, 255, brightness);  // Blue
    }
    FastLED.show();
}
```

## Variations

**Rainbow Breathing:**
```cpp
void rainbowBreathe() {
    static uint8_t hue = 0;
    uint8_t brightness = beatsin8(12);

    fill_rainbow(leds, NUM_LEDS, hue, 255 / NUM_LEDS);

    for (int i = 0; i < NUM_LEDS; i++) {
        leds[i].fadeToBlackBy(255 - brightness);
    }

    EVERY_N_MILLISECONDS(20) { hue++; }
}
```

**Sectioned Breathing:**
```cpp
void sectionBreathe() {
    int section1 = NUM_LEDS / 3;
    int section2 = (NUM_LEDS * 2) / 3;

    uint8_t b1 = beatsin8(10);
    uint8_t b2 = beatsin8(10, 0, 255, 0, 85);   // 1/3 phase shift
    uint8_t b3 = beatsin8(10, 0, 255, 0, 170);  // 2/3 phase shift

    fill_solid(&leds[0], section1, CHSV(0, 255, b1));
    fill_solid(&leds[section1], section2 - section1, CHSV(96, 255, b2));
    fill_solid(&leds[section2], NUM_LEDS - section2, CHSV(160, 255, b3));
}
```

**Heartbeat:**
```cpp
void heartbeat() {
    // Quick double-beat pattern
    static uint16_t beatTimer = 0;
    uint8_t brightness = 0;

    uint16_t beatPhase = beatTimer % 1000;

    if (beatPhase < 100) {
        brightness = map(beatPhase, 0, 100, 0, 255);
    } else if (beatPhase < 200) {
        brightness = map(beatPhase, 100, 200, 255, 0);
    } else if (beatPhase < 300) {
        brightness = map(beatPhase, 200, 300, 0, 255);
    } else if (beatPhase < 400) {
        brightness = map(beatPhase, 300, 400, 255, 0);
    }

    fill_solid(leds, NUM_LEDS, CRGB::Red);
    FastLED.setBrightness(brightness);
    FastLED.show();

    beatTimer += 20;
    delay(20);
}
```

## Tips

- Use slower BPM (6-10) for ambient/relaxing effects
- Use faster BPM (15-30) for alert/notification effects
- Combine with color changes for more dynamic effects
- Adjust min brightness to prevent complete darkness
- Use phase offsets to create traveling wave patterns
