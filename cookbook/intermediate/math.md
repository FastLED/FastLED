# Math and Mapping Functions

**Difficulty Level**: ⭐⭐ Intermediate
**Time to Complete**: 50-60 minutes
**Prerequisites**:
- [Animations](../basic-patterns/animations.md) - Understanding static variables and timing
- [Color Theory](../core-concepts/color-theory.md) - RGB and HSV color models
- [Timing Control](../core-concepts/timing.md) - Frame rates and animation control

**You'll Learn**:
- How to use FastLED's optimized integer math functions (scale8, qadd8, qsub8) for fast, safe calculations
- How to create smooth wave patterns with sin8/cos8 and animated effects
- How to generate rhythmic animations using beat8 and beatsin8 functions
- How to map values between ranges for brightness, position, and color control
- How to combine multiple mathematical functions for complex, layered effects

FastLED provides a rich set of mathematical functions optimized for LED animations. These functions are essential for creating precise, rhythmic, and mathematically beautiful patterns.

## Why Use FastLED Math Functions?

- **Optimized**: Faster than standard Arduino/C++ math functions
- **Integer-based**: Avoid slow floating-point operations
- **Saturation**: Automatic overflow protection
- **Consistent**: Same results across different platforms

## Scaling Values

### scale8()

Fast 8-bit scaling, essential for brightness control:

```cpp
// Scale value by multiplier (both 0-255)
uint8_t scaled = scale8(value, 128);  // value * 128 / 255

// Example: Dim a color to 50%
CRGB color = CRGB::Red;
color.r = scale8(color.r, 128);
color.g = scale8(color.g, 128);
color.b = scale8(color.b, 128);
```

**Common scale8 values:**
- `255` = 100% (no change)
- `128` = 50%
- `64` = 25%
- `32` = 12.5%

### scale16()

For 16-bit values when you need more precision:

```cpp
uint16_t scaled16 = scale16(value, 32768);  // value * 32768 / 65535
```

### nscale8()

Scale in-place (modifies the original):

```cpp
CRGB color = CRGB::Blue;
color.nscale8(128);  // Dims to 50%
```

## Mapping Values

### map()

Map a value from one range to another:

```cpp
// Map analog reading (0-1023) to LED brightness (0-255)
uint8_t brightness = map(analogRead(A0), 0, 1023, 0, 255);

// Map LED position to hue range
uint8_t hue = map(i, 0, NUM_LEDS-1, 0, 255);
```

### scale8_video()

Similar to scale8 but preserves more detail in darker values:

```cpp
// Better for dimming without losing too much color
uint8_t videoScaled = scale8_video(brightness, 64);
```

## Saturation Math

These functions prevent overflow/underflow:

### qadd8()

Add with saturation (max 255):

```cpp
uint8_t result = qadd8(200, 100);  // Results in 255, not overflow

// Example: Brighten color safely
leds[0] = CRGB::Red;
leds[0].r = qadd8(leds[0].r, 50);  // Won't overflow past 255
```

### qsub8()

Subtract with saturation (min 0):

```cpp
uint8_t result = qsub8(50, 100);  // Results in 0, not underflow

// Example: Dim color safely
leds[0].r = qsub8(leds[0].r, 30);  // Won't go below 0
```

### qadd16() and qsub16()

16-bit versions for larger values:

```cpp
uint16_t sum = qadd16(60000, 10000);  // Saturates at 65535
uint16_t diff = qsub16(100, 1000);    // Saturates at 0
```

## Wave Functions

### sin8()

Fast 8-bit sine wave (0-255 input, 0-255 output):

```cpp
void sineWave() {
    for (int i = 0; i < NUM_LEDS; i++) {
        // Create sine wave pattern
        uint8_t brightness = sin8(i * 255 / NUM_LEDS);
        leds[i] = CHSV(160, 255, brightness);
    }
}
```

**Key points:**
- Input range: 0-255 (0 = 0°, 255 = 360°)
- Output range: 0-255 (0 = -1.0, 128 = 0, 255 = +1.0)
- Much faster than `sin()` or `sinf()`

### cos8()

Cosine version (90° phase shift from sin8):

```cpp
uint8_t cosValue = cos8(angle);
```

### sin16()

16-bit sine for higher precision:

```cpp
// Input: 0-65535, Output: -32768 to +32767
int16_t sineValue = sin16(angle);
```

### Animated Sine Wave

```cpp
void animatedSine() {
    static uint8_t phase = 0;

    for (int i = 0; i < NUM_LEDS; i++) {
        uint8_t brightness = sin8(phase + (i * 10));
        leds[i] = CHSV(160, 255, brightness);
    }

    phase += 4;  // Wave speed
}
```

## Beat Functions

Create rhythmic patterns with beat generators:

### beat8()

Sawtooth wave that increases from 0 to 255:

```cpp
// Returns value that increases over time based on BPM
uint8_t beat = beat8(60);  // 60 beats per minute

void beatEffect() {
    uint8_t hue = beat8(20);  // Slow color change
    fill_solid(leds, NUM_LEDS, CHSV(hue, 255, 255));
}
```

### beat16()

16-bit version (0-65535):

```cpp
uint16_t beat = beat16(60);
```

### beatsin8()

Sine wave that oscillates smoothly:

```cpp
// beatsin8(BPM, min_value, max_value)
uint8_t brightness = beatsin8(10, 50, 200);  // Oscillates between 50-200

void pulseEffect() {
    uint8_t brightness = beatsin8(12);  // 12 BPM, 0-255
    fill_solid(leds, NUM_LEDS, CHSV(160, 255, brightness));
}
```

### beatsin8() with Phase

Create wave patterns across LEDs:

```cpp
void sineWaveAcrossStrip() {
    for (int i = 0; i < NUM_LEDS; i++) {
        uint8_t brightness = beatsin8(
            60,              // Beats per minute
            0,               // Minimum value
            255,             // Maximum value
            0,               // Time offset
            i * (255 / NUM_LEDS)  // Phase offset per LED
        );

        leds[i] = CHSV(160, 255, brightness);
    }
}
```

### beatsin16()

16-bit precision oscillation:

```cpp
uint16_t value = beatsin16(60, 0, 1000);  // Oscillates 0-1000
```

## Random Functions

### random8()

Fast random number generation:

```cpp
uint8_t randomValue = random8();          // 0-255
uint8_t randomRange = random8(10, 50);    // 10-49
uint8_t randomMax = random8(100);         // 0-99
```

### random16()

16-bit random numbers:

```cpp
uint16_t random16Value = random16();      // 0-65535
uint16_t randomPos = random16(NUM_LEDS);  // 0 to NUM_LEDS-1
```

### random8() for Effects

```cpp
void randomTwinkle() {
    if (random8() < 50) {  // 50/255 chance (~20%)
        int pos = random16(NUM_LEDS);
        leds[pos] = CHSV(random8(), 255, 255);
    }
}
```

## Practical Applications

### Breathing Effect

```cpp
void breathe() {
    // Smooth breathing using beatsin8
    uint8_t brightness = beatsin8(12, 30, 255);  // 12 BPM, range 30-255

    for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = CHSV(160, 255, brightness);
    }
}
```

### Wave Chaser

```cpp
void waveChaser() {
    static uint8_t offset = 0;

    for (int i = 0; i < NUM_LEDS; i++) {
        uint8_t brightness = sin8(offset + (i * 20));
        leds[i] = CHSV(0, 255, brightness);
    }

    offset += 5;
}
```

### Multiple Wave Interference

```cpp
void multiWave() {
    for (int i = 0; i < NUM_LEDS; i++) {
        // Combine three waves with different speeds
        uint8_t wave1 = beatsin8(10, 0, 255, 0, i * 8);
        uint8_t wave2 = beatsin8(15, 0, 255, 0, i * 12);
        uint8_t wave3 = beatsin8(20, 0, 255, 0, i * 16);

        uint8_t combined = (wave1 + wave2 + wave3) / 3;
        leds[i] = CHSV(combined, 255, 255);
    }
}
```

### Position Mapping

```cpp
void positionGradient() {
    for (int i = 0; i < NUM_LEDS; i++) {
        // Map position to full hue range
        uint8_t hue = map(i, 0, NUM_LEDS-1, 0, 255);
        leds[i] = CHSV(hue, 255, 255);
    }
}
```

### Brightness Scaling

```cpp
void dimEnds() {
    CRGB baseColor = CRGB::Blue;

    for (int i = 0; i < NUM_LEDS; i++) {
        // Brighter in center, dimmer at ends
        uint8_t distanceFromCenter = abs(i - NUM_LEDS/2);
        uint8_t brightness = 255 - scale8(distanceFromCenter, 4);

        leds[i] = baseColor;
        leds[i].nscale8(brightness);
    }
}
```

## Easing Functions

Create natural acceleration/deceleration:

### Quadratic Easing

```cpp
uint8_t easeInQuad(uint8_t t) {
    // Accelerating from zero velocity
    uint16_t t16 = t;
    return ((t16 * t16) >> 8);
}

uint8_t easeOutQuad(uint8_t t) {
    // Decelerating to zero velocity
    uint16_t t16 = 255 - t;
    return 255 - ((t16 * t16) >> 8);
}
```

### Cubic Easing

```cpp
uint8_t easeInOutCubic(uint8_t t) {
    if (t < 128) {
        // Accelerate (first half)
        return ((uint16_t)t * t * t) >> 14;
    } else {
        // Decelerate (second half)
        uint16_t t2 = 255 - t;
        return 255 - (((t2 * t2 * t2) >> 14));
    }
}
```

### Using Easing

```cpp
void easedMovement() {
    static uint8_t time = 0;

    // Apply easing to position
    uint8_t easedTime = easeInOutCubic(time);
    uint8_t pos = map(easedTime, 0, 255, 0, NUM_LEDS-1);

    FastLED.clear();
    leds[pos] = CRGB::White;

    time += 2;
}
```

## Bit Manipulation

### Bitwise Shifts (Fast Division/Multiplication)

```cpp
// Division by powers of 2
uint8_t half = value >> 1;      // Divide by 2
uint8_t quarter = value >> 2;   // Divide by 4
uint8_t eighth = value >> 3;    // Divide by 8

// Multiplication by powers of 2
uint8_t doubled = value << 1;   // Multiply by 2
uint8_t quadruple = value << 2; // Multiply by 4
```

### Modulo Power of 2

```cpp
// Fast modulo for powers of 2
uint8_t wrapped = index & 0xFF;  // Same as % 256
uint8_t masked = index & 0x0F;   // Same as % 16
```

## Advanced Math Examples

### Circular Motion

```cpp
void circularMotion() {
    static uint8_t angle = 0;

    // Calculate position using sine and cosine
    int8_t x = sin8(angle) - 128;        // -128 to 127
    int8_t y = cos8(angle) - 128;        // -128 to 127

    // Map to LED position
    uint8_t pos = map(x, -128, 127, 0, NUM_LEDS-1);

    FastLED.clear();
    leds[pos] = CRGB::Cyan;

    angle += 4;
}
```

### Distance-Based Brightness

```cpp
void distanceFade() {
    uint8_t center = NUM_LEDS / 2;

    for (int i = 0; i < NUM_LEDS; i++) {
        // Calculate distance from center
        uint8_t distance = abs(i - center);

        // Fade based on distance
        uint8_t brightness = qsub8(255, scale8(distance, 20));

        leds[i] = CHSV(160, 255, brightness);
    }
}
```

### Harmonic Motion

```cpp
void harmonicMotion() {
    // Combine multiple sine waves (harmonics)
    for (int i = 0; i < NUM_LEDS; i++) {
        uint8_t fundamental = beatsin8(10, 0, 255, 0, i * 16);
        uint8_t harmonic2 = beatsin8(20, 0, 128, 0, i * 16);
        uint8_t harmonic3 = beatsin8(30, 0, 64, 0, i * 16);

        uint8_t combined = qadd8(qadd8(fundamental, harmonic2), harmonic3);
        leds[i] = CHSV(i * 4, 255, combined);
    }
}
```

## Performance Tips

1. **Use integer math**: Avoid floating-point when possible
2. **Pre-calculate**: Move calculations outside loops when you can
3. **Bitwise operations**: Use shifts instead of multiply/divide by powers of 2
4. **Lookup tables**: For complex functions used repeatedly

### Example Optimization

```cpp
// SLOW: Floating point in loop
void slowVersion() {
    for (int i = 0; i < NUM_LEDS; i++) {
        float angle = (i * 360.0) / NUM_LEDS;
        float brightness = sin(angle * PI / 180.0) * 255;
        leds[i].b = (uint8_t)brightness;
    }
}

// FAST: Integer math with sin8
void fastVersion() {
    for (int i = 0; i < NUM_LEDS; i++) {
        uint8_t angle = (i * 255) / NUM_LEDS;
        uint8_t brightness = sin8(angle);
        leds[i].b = brightness;
    }
}
```

## Common Patterns Reference

```cpp
// Pulse
uint8_t pulse = beatsin8(BPM, MIN, MAX);

// Sawtooth
uint8_t saw = beat8(BPM);

// Triangle wave
uint8_t triangle = triwave8(angle);

// Square wave
uint8_t square = angle < 128 ? 0 : 255;

// Smooth step
uint8_t smoothed = ease8InOutCubic(linear);
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
    multiWaveEffect();
    FastLED.show();
    FastLED.delay(1000 / 60);
}

void multiWaveEffect() {
    // Create complex pattern using multiple wave functions
    for (int i = 0; i < NUM_LEDS; i++) {
        // Wave 1: Slow brightness oscillation
        uint8_t brightness = beatsin8(8, 100, 255, 0, i * 4);

        // Wave 2: Color rotation
        uint8_t hue = beat8(20) + (i * 4);

        // Wave 3: Position-based sine wave
        uint8_t wave = sin8(i * 20 + beat8(30));

        // Combine for final effect
        uint8_t finalBrightness = scale8(brightness, wave);
        leds[i] = CHSV(hue, 255, finalBrightness);
    }
}
```

## See Also

- [Blending and Transitions](blending.md) - Use math functions to control blending
- [Noise and Perlin Noise](noise.md) - Combine with noise for organic effects
- [Color Palettes](palettes.md) - Use beat functions to animate through palettes
