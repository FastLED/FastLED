# FastLED Cookbook

A practical guide to creating LED effects with FastLED.

## Table of Contents

1. [Getting Started](#getting-started)
   - [Installation and Setup](#installation-and-setup)
   - [Hardware Requirements](#hardware-requirements)
   - [Basic Concepts](#basic-concepts)
   - [Your First Blink Example](#your-first-blink-example)

2. [Core Concepts](#core-concepts)
   - [Understanding LED Data Structures](#understanding-led-data-structures)
   - [Color Theory](#color-theory)
   - [Timing and Frame Rate](#timing-and-frame-rate)
   - [Power Considerations](#power-considerations)

3. [Basic Patterns](#basic-patterns)
   - [Solid Colors](#solid-colors)
   - [Simple Animations](#simple-animations)
   - [Chases and Scanners](#chases-and-scanners)
   - [Rainbow Effects](#rainbow-effects)

4. [Intermediate Techniques](#intermediate-techniques)
   - [Color Palettes](#color-palettes)
   - [Noise and Perlin Noise](#noise-and-perlin-noise)
   - [Blending and Transitions](#blending-and-transitions)
   - [Math and Mapping Functions](#math-and-mapping-functions)

5. [Advanced Effects](#advanced-effects)
   - [2D/Matrix Operations](#2d-matrix-operations)
   - [Audio Reactive Patterns](#audio-reactive-patterns)
   - [Multiple Strip Coordination](#multiple-strip-coordination)
   - [Performance Optimization](#performance-optimization)

6. [Common Recipes](#common-recipes)
   - [Fire Effect](#fire-effect)
   - [Twinkle/Sparkle](#twinkle-sparkle)
   - [Breathing Effect](#breathing-effect)
   - [Wave Patterns](#wave-patterns)

7. [Troubleshooting](#troubleshooting)
   - [Common Issues and Solutions](#common-issues-and-solutions)
   - [Debugging Tips](#debugging-tips)
   - [Hardware Problems](#hardware-problems)

8. [Reference](#reference)
   - [API Quick Reference](#api-quick-reference)
   - [Pin Configurations by Platform](#pin-configurations-by-platform)
   - [Supported LED Types](#supported-led-types)

---

## Getting Started

### Installation and Setup

**PlatformIO (Recommended)**
```ini
[env:myboard]
platform = ...
lib_deps =
    fastled/FastLED
```

**Arduino IDE**
1. Open Library Manager (Sketch → Include Library → Manage Libraries)
2. Search for "FastLED"
3. Click Install

### Hardware Requirements

- **Microcontroller**: Arduino, ESP32, ESP8266, Teensy, STM32, etc.
- **LED Strip**: WS2812B, APA102, SK6812, or other supported types
- **Power Supply**: 5V power supply sized for your LED count (estimate 60mA per LED at full white)
- **Capacitor**: 1000µF capacitor across power supply (recommended)
- **Resistor**: 220-470Ω resistor on data line (recommended)

### Basic Concepts

**LED Addressing**: Each LED in your strip has an index starting from 0. You control them by setting color values in an array.

**Color Models**:
- **RGB**: Red, Green, Blue (0-255 each)
- **HSV**: Hue (0-255), Saturation (0-255), Value/Brightness (0-255)

**The Main Loop Pattern**:
```cpp
void loop() {
    // 1. Calculate/update LED colors
    // 2. Call FastLED.show()
    // 3. Optional: delay or manage frame rate
}
```

### Your First Blink Example

```cpp
#include <FastLED.h>

#define LED_PIN     5
#define NUM_LEDS    60
#define LED_TYPE    WS2812B
#define COLOR_ORDER GRB

CRGB leds[NUM_LEDS];

void setup() {
    FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
    FastLED.setBrightness(50);  // 0-255
}

void loop() {
    // Turn all LEDs red
    fill_solid(leds, NUM_LEDS, CRGB::Red);
    FastLED.show();
    delay(1000);

    // Turn all LEDs off
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    FastLED.show();
    delay(1000);
}
```

---

## Core Concepts

### Understanding LED Data Structures

**CRGB Array**: The fundamental data structure
```cpp
CRGB leds[NUM_LEDS];  // Array of RGB color values

// Access individual LEDs
leds[0] = CRGB::Red;           // First LED
leds[5] = CRGB(255, 0, 128);   // Direct RGB values
leds[10].r = 200;              // Access color channels
```

### Color Theory

**RGB Colors** (Red, Green, Blue):
```cpp
CRGB color1 = CRGB(255, 0, 0);      // Red
CRGB color2 = CRGB(0, 255, 0);      // Green
CRGB color3 = CRGB(0, 0, 255);      // Blue
CRGB color4 = CRGB(255, 255, 255);  // White
```

**HSV Colors** (Hue, Saturation, Value):
```cpp
CHSV hsv1 = CHSV(0, 255, 255);      // Red
CHSV hsv2 = CHSV(96, 255, 255);     // Green
CHSV hsv3 = CHSV(160, 255, 255);    // Blue

// Convert HSV to RGB
leds[0] = hsv1;  // Automatic conversion
```

**Why Use HSV?**
- Easier to create smooth color transitions (just increment hue)
- Natural for rainbow effects
- Intuitive control of brightness without changing color

### Timing and Frame Rate

**Simple Delay**:
```cpp
void loop() {
    updateLEDs();
    FastLED.show();
    delay(20);  // ~50 FPS
}
```

**Frame Rate Control**:
```cpp
void loop() {
    EVERY_N_MILLISECONDS(20) {  // 50 FPS
        updateLEDs();
        FastLED.show();
    }
}
```

**Non-blocking Timing**:
```cpp
uint32_t lastUpdate = 0;
const uint32_t updateInterval = 20;

void loop() {
    uint32_t now = millis();
    if (now - lastUpdate >= updateInterval) {
        lastUpdate = now;
        updateLEDs();
        FastLED.show();
    }
}
```

### Power Considerations

- **Rule of thumb**: 60mA per LED at full white brightness
- **Example**: 100 LEDs × 60mA = 6A power supply needed
- **Reduce current draw**:
  - Lower brightness: `FastLED.setBrightness(50)`
  - Limit power: `FastLED.setMaxPowerInVoltsAndMilliamps(5, 2000)`
  - Use fewer LEDs at full brightness

---

## Basic Patterns

### Solid Colors

```cpp
// Fill entire strip
fill_solid(leds, NUM_LEDS, CRGB::Blue);

// Fill range
fill_solid(&leds[10], 20, CRGB::Green);  // LEDs 10-29

// Fill with HSV
fill_solid(leds, NUM_LEDS, CHSV(160, 255, 255));
```

### Simple Animations

**Fade In/Out**:
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

### Chases and Scanners

**Nightrider / Cylon Effect**:
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

See also: [examples/Cylon](examples/Cylon)

### Rainbow Effects

**Static Rainbow**:
```cpp
void rainbow() {
    fill_rainbow(leds, NUM_LEDS, 0, 255 / NUM_LEDS);
}
```

**Moving Rainbow**:
```cpp
void movingRainbow() {
    static uint8_t hue = 0;
    fill_rainbow(leds, NUM_LEDS, hue, 255 / NUM_LEDS);
    hue++;
}
```

---

## Intermediate Techniques

### Color Palettes

Color palettes allow you to define a set of colors and smoothly blend between them.

**Predefined Palettes**:
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

**Custom Palettes**:
```cpp
// Define colors at specific positions
DEFINE_GRADIENT_PALETTE(sunset_gp) {
    0,   255, 0,   0,    // Red at position 0
    128, 255, 128, 0,    // Orange at position 128
    255, 255, 255, 0     // Yellow at position 255
};

CRGBPalette16 sunsetPalette = sunset_gp;
```

**Using Palettes**:
```cpp
void paletteEffect() {
    static uint8_t paletteIndex = 0;

    for (int i = 0; i < NUM_LEDS; i++) {
        uint8_t colorIndex = paletteIndex + (i * 4);
        leds[i] = ColorFromPalette(palette, colorIndex, 255, LINEARBLEND);
    }

    paletteIndex++;  // Animate through palette
}
```

### Noise and Perlin Noise

FastLED includes noise functions for organic, natural-looking animations.

**Basic Noise**:
```cpp
void noiseEffect() {
    static uint16_t x = 0;

    for (int i = 0; i < NUM_LEDS; i++) {
        // Generate noise value (0-255)
        uint8_t noise = inoise8(x + (i * 100), millis() / 10);

        // Map to color
        leds[i] = CHSV(noise, 255, 255);
    }

    x += 10;
}
```

**2D Noise for Movement**:
```cpp
void lavaLamp() {
    for (int i = 0; i < NUM_LEDS; i++) {
        // Create time-varying noise
        uint8_t noise = inoise8(
            i * 50,                    // X position
            millis() / 20 + i * 10     // Y position (time-based)
        );

        // Use with heat colors palette
        leds[i] = ColorFromPalette(HeatColors_p, noise);
    }
}
```

**Noise Parameters**:
- Larger position increments = more chaotic/faster changes
- Smaller increments = smoother transitions
- Time component (millis()) creates animation

### Blending and Transitions

**Fade Between Colors**:
```cpp
void smoothTransition() {
    static uint8_t blendAmount = 0;
    CRGB color1 = CRGB::Red;
    CRGB color2 = CRGB::Blue;

    for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = blend(color1, color2, blendAmount);
    }

    blendAmount++;
}
```

**Fade to Black (Trails)**:
```cpp
void trailEffect() {
    // Fade all LEDs toward black
    fadeToBlackBy(leds, NUM_LEDS, 20);  // 20 = fade speed

    // Add new LED
    static uint8_t pos = 0;
    leds[pos] = CRGB::White;
    pos = (pos + 1) % NUM_LEDS;
}
```

**Blur for Smoothing**:
```cpp
void blurExample() {
    // Set some random LEDs
    leds[random8(NUM_LEDS)] = CHSV(random8(), 255, 255);

    // Blur to create smooth gradients
    blur1d(leds, NUM_LEDS, 50);  // 50 = blur amount
}
```

**Cross-Fade Between Patterns**:
```cpp
CRGB pattern1[NUM_LEDS];
CRGB pattern2[NUM_LEDS];

void crossFade(uint8_t amount) {
    for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = blend(pattern1[i], pattern2[i], amount);
    }
}
```

### Math and Mapping Functions

**Scaling Values**:
```cpp
// Map value from one range to another
uint8_t mapped = map(value, 0, 1023, 0, 255);

// Scale8 - fast 8-bit scaling
uint8_t scaled = scale8(value, 128);  // value * 128 / 255

// Scale16 - for 16-bit values
uint16_t scaled16 = scale16(value, 32768);
```

**Wave Functions**:
```cpp
void sineWave() {
    for (int i = 0; i < NUM_LEDS; i++) {
        // Create sine wave
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

**Easing Functions**:
```cpp
// Quadratic easing (acceleration)
uint8_t easeInQuad(uint8_t t) {
    uint16_t t16 = t;
    return ((t16 * t16) >> 8);
}

// Cubic easing (smooth acceleration)
uint8_t easeInOutCubic(uint8_t t) {
    if (t < 128) {
        return ((uint16_t)t * t * t) >> 14;
    } else {
        uint16_t t2 = 255 - t;
        return 255 - (((t2 * t2 * t2) >> 14));
    }
}
```

**Random Functions**:
```cpp
uint8_t randomValue = random8();          // 0-255
uint8_t randomRange = random8(10, 50);    // 10-49
uint16_t random16Value = random16();      // 0-65535
```

**Beat Functions**:
```cpp
// Sawtooth wave (0-255 repeating)
uint8_t beat = beat8(60);  // 60 BPM

// Sine wave (0-255 oscillating)
uint8_t sineBeat = beatsin8(60);

// Sine wave with range
uint8_t rangedBeat = beatsin8(60, 50, 200);  // Oscillates 50-200
```

---

## Advanced Effects

### 2D/Matrix Operations

When working with LED matrices or grids, you need to map 2D coordinates to a 1D array.

**XY Mapping Function**:
```cpp
#define MATRIX_WIDTH  16
#define MATRIX_HEIGHT 16
#define NUM_LEDS (MATRIX_WIDTH * MATRIX_HEIGHT)

CRGB leds[NUM_LEDS];

// Convert X,Y to array index
uint16_t XY(uint8_t x, uint8_t y) {
    // Handle out of bounds
    if (x >= MATRIX_WIDTH || y >= MATRIX_HEIGHT) {
        return 0;
    }

    // Serpentine (zigzag) layout
    if (y & 0x01) {
        // Odd rows run backwards
        uint8_t reverseX = (MATRIX_WIDTH - 1) - x;
        return (y * MATRIX_WIDTH) + reverseX;
    } else {
        // Even rows run forwards
        return (y * MATRIX_WIDTH) + x;
    }
}
```

**2D Patterns**:
```cpp
void plasma() {
    for (uint8_t x = 0; x < MATRIX_WIDTH; x++) {
        for (uint8_t y = 0; y < MATRIX_HEIGHT; y++) {
            // Create 2D sine wave pattern
            uint8_t brightness = qsub8(
                inoise8(x * 30, y * 30, millis() / 10),
                inoise8(x * 50, y * 50, millis() / 20)
            );

            uint16_t index = XY(x, y);
            leds[index] = CHSV(brightness, 255, 255);
        }
    }
}
```

**Moving Shapes**:
```cpp
void bouncingBall() {
    static float ballX = MATRIX_WIDTH / 2.0;
    static float ballY = MATRIX_HEIGHT / 2.0;
    static float velocityX = 0.3;
    static float velocityY = 0.2;

    // Clear matrix
    fadeToBlackBy(leds, NUM_LEDS, 50);

    // Update position
    ballX += velocityX;
    ballY += velocityY;

    // Bounce off walls
    if (ballX <= 0 || ballX >= MATRIX_WIDTH - 1) velocityX = -velocityX;
    if (ballY <= 0 || ballY >= MATRIX_HEIGHT - 1) velocityY = -velocityY;

    // Draw ball
    leds[XY((uint8_t)ballX, (uint8_t)ballY)] = CRGB::White;
}
```

### Audio Reactive Patterns

Read audio input and visualize it with LEDs.

**Basic Audio Setup** (requires analog input):
```cpp
#define MIC_PIN A0
#define SAMPLE_WINDOW 50  // Sample window width in ms

uint16_t readAudioLevel() {
    unsigned long startMillis = millis();
    uint16_t peakToPeak = 0;
    uint16_t signalMax = 0;
    uint16_t signalMin = 1024;

    // Collect data for sample window
    while (millis() - startMillis < SAMPLE_WINDOW) {
        uint16_t sample = analogRead(MIC_PIN);
        if (sample > signalMax) signalMax = sample;
        if (sample < signalMin) signalMin = sample;
    }

    peakToPeak = signalMax - signalMin;
    return peakToPeak;
}
```

**VU Meter Effect**:
```cpp
void vuMeter() {
    uint16_t audioLevel = readAudioLevel();

    // Map to LED count (0 to NUM_LEDS)
    uint8_t numLEDsToLight = map(audioLevel, 0, 1023, 0, NUM_LEDS);

    // Clear all
    fill_solid(leds, NUM_LEDS, CRGB::Black);

    // Light up LEDs based on level
    for (int i = 0; i < numLEDsToLight; i++) {
        // Color gradient: green -> yellow -> red
        uint8_t hue = map(i, 0, NUM_LEDS, 96, 0);  // Green to red
        leds[i] = CHSV(hue, 255, 255);
    }
}
```

**Beat Detection**:
```cpp
class BeatDetector {
private:
    uint16_t samples[32];
    uint8_t sampleIndex = 0;
    uint32_t lastBeatTime = 0;

public:
    bool detectBeat(uint16_t audioLevel) {
        // Store sample
        samples[sampleIndex] = audioLevel;
        sampleIndex = (sampleIndex + 1) % 32;

        // Calculate average
        uint32_t sum = 0;
        for (int i = 0; i < 32; i++) sum += samples[i];
        uint16_t average = sum / 32;

        // Detect beat (current level significantly above average)
        bool isBeat = (audioLevel > average * 1.5) &&
                      (millis() - lastBeatTime > 300);  // Minimum 300ms between beats

        if (isBeat) lastBeatTime = millis();
        return isBeat;
    }
};

BeatDetector beatDetector;

void beatEffect() {
    uint16_t audioLevel = readAudioLevel();

    if (beatDetector.detectBeat(audioLevel)) {
        // Flash on beat
        fill_solid(leds, NUM_LEDS, CRGB::White);
    } else {
        // Fade out
        fadeToBlackBy(leds, NUM_LEDS, 10);
    }
}
```

### Multiple Strip Coordination

Control multiple LED strips independently or in coordination.

**Multiple Controllers**:
```cpp
#define NUM_STRIPS 3
#define LEDS_PER_STRIP 60

CRGB strip1[LEDS_PER_STRIP];
CRGB strip2[LEDS_PER_STRIP];
CRGB strip3[LEDS_PER_STRIP];

void setup() {
    FastLED.addLeds<WS2812B, 5, GRB>(strip1, LEDS_PER_STRIP);
    FastLED.addLeds<WS2812B, 6, GRB>(strip2, LEDS_PER_STRIP);
    FastLED.addLeds<WS2812B, 7, GRB>(strip3, LEDS_PER_STRIP);
}
```

**Synchronized Effects**:
```cpp
void syncedRainbow() {
    static uint8_t hue = 0;

    // Same pattern on all strips
    fill_rainbow(strip1, LEDS_PER_STRIP, hue, 5);
    fill_rainbow(strip2, LEDS_PER_STRIP, hue, 5);
    fill_rainbow(strip3, LEDS_PER_STRIP, hue, 5);

    hue++;
}
```

**Wave Across Strips**:
```cpp
void waveAcrossStrips() {
    static uint8_t pos = 0;

    // Calculate position on virtual combined strip
    uint16_t totalLEDs = NUM_STRIPS * LEDS_PER_STRIP;
    uint16_t virtualPos = map(pos, 0, 255, 0, totalLEDs);

    // Clear all
    fill_solid(strip1, LEDS_PER_STRIP, CRGB::Black);
    fill_solid(strip2, LEDS_PER_STRIP, CRGB::Black);
    fill_solid(strip3, LEDS_PER_STRIP, CRGB::Black);

    // Determine which strip and position
    if (virtualPos < LEDS_PER_STRIP) {
        strip1[virtualPos] = CRGB::White;
    } else if (virtualPos < LEDS_PER_STRIP * 2) {
        strip2[virtualPos - LEDS_PER_STRIP] = CRGB::White;
    } else {
        strip3[virtualPos - LEDS_PER_STRIP * 2] = CRGB::White;
    }

    pos++;
}
```

### Performance Optimization

Tips for smooth, efficient LED animations.

**Reduce Calculations in Loop**:
```cpp
// BAD: Recalculating every frame
void slowEffect() {
    for (int i = 0; i < NUM_LEDS; i++) {
        float angle = (i * 360.0) / NUM_LEDS;  // Expensive!
        // ...
    }
}

// GOOD: Precalculate once
uint8_t ledAngles[NUM_LEDS];

void setup() {
    for (int i = 0; i < NUM_LEDS; i++) {
        ledAngles[i] = (i * 255) / NUM_LEDS;
    }
}
```

**Use Integer Math**:
```cpp
// BAD: Float division
float result = (value * 3.14159) / 2.0;

// GOOD: Integer operations
uint8_t result = (value * 3) >> 1;  // Divide by 2 using bit shift
```

**Limit Frame Rate**:
```cpp
void loop() {
    EVERY_N_MILLISECONDS(20) {  // Max 50 FPS
        updateLEDs();
        FastLED.show();
    }
    // Other tasks can run here without blocking
}
```

**Power Management**:
```cpp
void setup() {
    // Limit total power draw
    FastLED.setMaxPowerInVoltsAndMilliamps(5, 2000);  // 5V, 2A max
}
```

**Efficient Array Operations**:
```cpp
// BAD: Individual assignments
for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CRGB::Black;
}

// GOOD: Use FastLED functions
fill_solid(leds, NUM_LEDS, CRGB::Black);
// or even faster:
FastLED.clear();
```

**Temporal Dithering** (reduces visible banding):
```cpp
void setup() {
    FastLED.setDither(BINARY_DITHER);  // or DISABLE_DITHER for speed
}
```

---

## Common Recipes

### Fire Effect

Realistic fire simulation using heat array and color palette.

```cpp
#define NUM_LEDS 60
CRGB leds[NUM_LEDS];
byte heat[NUM_LEDS];

void fire() {
    // Step 1: Cool down every cell a little
    for (int i = 0; i < NUM_LEDS; i++) {
        heat[i] = qsub8(heat[i], random8(0, ((55 * 10) / NUM_LEDS) + 2));
    }

    // Step 2: Heat from each cell drifts 'up' and diffuses a little
    for (int k = NUM_LEDS - 1; k >= 2; k--) {
        heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2]) / 3;
    }

    // Step 3: Randomly ignite new 'sparks' at the bottom
    if (random8() < 120) {
        int y = random8(7);
        heat[y] = qadd8(heat[y], random8(160, 255));
    }

    // Step 4: Map heat to LED colors
    for (int j = 0; j < NUM_LEDS; j++) {
        // Scale heat to 0-240 for palette index
        byte colorIndex = scale8(heat[j], 240);

        // Use HeatColors palette
        leds[j] = ColorFromPalette(HeatColors_p, colorIndex);
    }
}

void loop() {
    fire();
    FastLED.show();
    delay(15);
}
```

**Customizable Fire**:
```cpp
void customFire(uint8_t cooling, uint8_t sparking) {
    // cooling: 20-100 (higher = faster cooling)
    // sparking: 50-200 (higher = more sparks)

    // Cool down
    for (int i = 0; i < NUM_LEDS; i++) {
        heat[i] = qsub8(heat[i], random8(0, ((cooling * 10) / NUM_LEDS) + 2));
    }

    // Heat drift upward
    for (int k = NUM_LEDS - 1; k >= 2; k--) {
        heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2]) / 3;
    }

    // Ignite sparks
    if (random8() < sparking) {
        int y = random8(7);
        heat[y] = qadd8(heat[y], random8(160, 255));
    }

    // Map to colors
    for (int j = 0; j < NUM_LEDS; j++) {
        leds[j] = ColorFromPalette(HeatColors_p, scale8(heat[j], 240));
    }
}
```

### Twinkle/Sparkle

Random twinkling stars effect.

**Simple Twinkle**:
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

**Colored Twinkle**:
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

**Sparkle (instant flash)**:
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

**White Twinkle (Christmas lights)**:
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

### Breathing Effect

Smooth pulsing/breathing animation.

**Simple Breathing**:
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

**Multi-Color Breathing**:
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

**Phased Breathing** (LEDs breathe in sequence):
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

**Dual Breathing** (two colors alternating):
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

### Wave Patterns

**Sine Wave**:
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

**Color Wave**:
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

**Multiple Waves**:
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

**Pacifica (Ocean Waves)**:
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

**Ripple Effect**:
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

---

## Troubleshooting

### Common Issues and Solutions

**LEDs don't light up**:
- Check power connections (GND and 5V)
- Verify data pin matches code
- Try reducing brightness
- Check LED type and color order match your strip

**Wrong colors**:
- Try different COLOR_ORDER values (GRB, RGB, BGR, etc.)
- Common for WS2812B: use GRB

**Flickering**:
- Add capacitor across power supply
- Ensure solid ground connection
- Check power supply capacity

**First LED wrong, rest OK**:
- Add 220-470Ω resistor on data line
- Check data pin connection

### Debugging Tips

**Serial Monitoring**:
```cpp
void setup() {
    Serial.begin(115200);
    while (!Serial) { }  // Wait for serial (Leonardo/Micro only)

    Serial.println("FastLED Starting...");

    FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
    Serial.println("LEDs initialized");
}

void loop() {
    // Print diagnostic info
    EVERY_N_SECONDS(5) {
        Serial.print("FPS: ");
        Serial.println(FastLED.getFPS());
    }
}
```

**Test Pattern to Verify Setup**:
```cpp
void testPattern() {
    // Test RED
    fill_solid(leds, NUM_LEDS, CRGB::Red);
    FastLED.show();
    delay(1000);

    // Test GREEN
    fill_solid(leds, NUM_LEDS, CRGB::Green);
    FastLED.show();
    delay(1000);

    // Test BLUE
    fill_solid(leds, NUM_LEDS, CRGB::Blue);
    FastLED.show();
    delay(1000);

    // Test individual LEDs
    FastLED.clear();
    for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = CRGB::White;
        FastLED.show();
        Serial.print("LED ");
        Serial.println(i);
        delay(100);
    }
}
```

**Memory Usage Check**:
```cpp
void setup() {
    Serial.begin(115200);

    // Calculate memory usage
    int ledMemory = NUM_LEDS * sizeof(CRGB);
    Serial.print("LED array uses: ");
    Serial.print(ledMemory);
    Serial.println(" bytes");

    // Check available RAM (AVR only)
    #ifdef __AVR__
    extern int __heap_start, *__brkval;
    int freeMemory;
    if ((int)__brkval == 0) {
        freeMemory = ((int)&freeMemory) - ((int)&__heap_start);
    } else {
        freeMemory = ((int)&freeMemory) - ((int)__brkval);
    }
    Serial.print("Free RAM: ");
    Serial.print(freeMemory);
    Serial.println(" bytes");
    #endif
}
```

**Debugging Color Issues**:
```cpp
void debugColors() {
    // Test if colors are correct
    leds[0] = CRGB(255, 0, 0);    // Should be RED
    leds[1] = CRGB(0, 255, 0);    // Should be GREEN
    leds[2] = CRGB(0, 0, 255);    // Should be BLUE
    FastLED.show();

    // If colors are wrong, try different COLOR_ORDER:
    // RGB, RBG, GRB, GBR, BRG, BGR
}
```

**Frame Rate Monitoring**:
```cpp
void loop() {
    updatePattern();
    FastLED.show();

    EVERY_N_SECONDS(1) {
        uint16_t fps = FastLED.getFPS();
        Serial.print("FPS: ");
        Serial.println(fps);

        if (fps < 30) {
            Serial.println("WARNING: Low frame rate!");
        }
    }
}
```

**Check for Data Corruption**:
```cpp
void validateLEDData() {
    bool hasError = false;

    for (int i = 0; i < NUM_LEDS; i++) {
        // Check for impossible values
        if (leds[i].r > 255 || leds[i].g > 255 || leds[i].b > 255) {
            Serial.print("ERROR at LED ");
            Serial.println(i);
            hasError = true;
        }
    }

    if (!hasError) {
        Serial.println("LED data OK");
    }
}
```

### Hardware Problems

**Voltage Drop Issues**:
```
Problem: Last LEDs are dim or wrong color
Solution:
- Use thicker power wires (18-22 AWG for short runs)
- Inject power every 60-100 LEDs on long strips
- Connect 5V and GND at both ends of strip
- Use separate power supply (don't power from microcontroller)
```

**Wiring Diagram** (recommended):
```
Power Supply (+5V) ----+---> LED Strip VCC
                       |
                       +---> Microcontroller VIN/5V (if needed)

Power Supply (GND) ----+---> LED Strip GND
                       |
                       +---> Microcontroller GND (MUST share ground!)

Microcontroller GPIO --[220Ω resistor]---> LED Strip DATA

Power Supply (+) --[1000µF capacitor]-- Power Supply (-)
```

**Problem: First LED Wrong Color**:
```
Cause: Signal integrity issue
Solutions:
- Add 220-470Ω resistor on data line (close to microcontroller)
- Keep data wire short (< 6 inches if possible)
- Use level shifter for 3.3V to 5V conversion
- Try lower data pin impedance
```

**Problem: Random Flickering**:
```
Causes & Solutions:
1. Insufficient power supply capacity
   - Use power supply rated 20% higher than needed
   - Example: 100 LEDs = 6A minimum, use 8A supply

2. Poor ground connection
   - Ensure microcontroller GND connects to LED strip GND
   - Use star grounding (all GNDs meet at one point)

3. Interference
   - Keep data wire away from power wires
   - Add 0.1µF ceramic capacitor near each LED strip section
   - Use shielded cable for data line in noisy environments

4. Software timing issues
   - Disable interrupts during FastLED.show()
   - Avoid WiFi/Bluetooth during LED updates on ESP32
```

**Problem: LEDs Don't Turn On**:
```
Checklist:
[ ] Power supply voltage correct (5V for most strips)
[ ] Power supply capacity sufficient (60mA per LED)
[ ] Data pin correct in code (matches wiring)
[ ] LED type correct (WS2812B, APA102, etc.)
[ ] COLOR_ORDER correct (try GRB if RGB doesn't work)
[ ] Strip not damaged (test with known-good controller)
[ ] FastLED.show() being called
[ ] Brightness not set to 0
```

**Problem: Strip Works Partially**:
```
Scenario: First N LEDs work, rest don't

Cause: LED failure or data signal degradation

Solutions:
1. Find the bad LED:
   - Test each LED one by one
   - Cut out failed LED and reconnect
   - Consider it's the last working LED that's damaged

2. Signal boost:
   - Add 220Ω resistor on data line
   - Use level shifter for 3.3V systems
   - Reduce strip length or split into sections
```

**ESP32 Specific Issues**:
```
Problem: LEDs glitch during WiFi activity

Solution:
// Disable WiFi during LED updates
void loop() {
    // Pause WiFi
    #ifdef ESP32
    esp_wifi_stop();
    #endif

    updateLEDs();
    FastLED.show();

    #ifdef ESP32
    esp_wifi_start();
    #endif

    // Do WiFi tasks
    delay(10);
}

// Or use separate task on second core
```

**Power Supply Selection Guide**:
```
LED Count | Max Current (Full White) | Recommended Supply
----------|---------------------------|-------------------
30        | 1.8A                     | 2.5A
60        | 3.6A                     | 5A
100       | 6.0A                     | 8A
150       | 9.0A                     | 12A
200       | 12.0A                    | 15A

Note: Full white is worst case. Most patterns use 30-50% of max.
Use setMaxPowerInVoltsAndMilliamps() to limit current draw.
```

**Testing Hardware**:
```cpp
// Comprehensive hardware test
void hardwareTest() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("=== FastLED Hardware Test ===");

    // Test 1: Single LED
    Serial.println("Test 1: First LED RED");
    FastLED.clear();
    leds[0] = CRGB::Red;
    FastLED.show();
    delay(2000);

    // Test 2: All RED
    Serial.println("Test 2: All LEDs RED");
    fill_solid(leds, NUM_LEDS, CRGB::Red);
    FastLED.show();
    delay(2000);

    // Test 3: Color test
    Serial.println("Test 3: Color sequence");
    CRGB colors[] = {CRGB::Red, CRGB::Green, CRGB::Blue, CRGB::White};
    for (int c = 0; c < 4; c++) {
        fill_solid(leds, NUM_LEDS, colors[c]);
        FastLED.show();
        delay(1000);
    }

    // Test 4: Individual LEDs
    Serial.println("Test 4: Individual LED scan");
    FastLED.clear();
    for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = CRGB::White;
        FastLED.show();
        delay(50);
        leds[i] = CRGB::Black;
    }

    Serial.println("=== Test Complete ===");
}
```

---

## Reference

### API Quick Reference

**Setup Functions**:
```cpp
// Add LEDs (single pin)
FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);

// Add LEDs with clock pin (APA102, etc.)
FastLED.addLeds<LED_TYPE, DATA_PIN, CLOCK_PIN, COLOR_ORDER>(leds, NUM_LEDS);

// Set global brightness (0-255)
FastLED.setBrightness(value);

// Set maximum power draw
FastLED.setMaxPowerInVoltsAndMilliamps(volts, milliamps);

// Set dithering mode
FastLED.setDither(BINARY_DITHER);  // or DISABLE_DITHER

// Set color correction
FastLED.setCorrection(TypicalLEDStrip);  // or TypicalPixelString

// Set color temperature
FastLED.setTemperature(Candle);  // or others
```

**Display Functions**:
```cpp
FastLED.show();              // Update LEDs
FastLED.clear();             // Set all to black
FastLED.clear(true);         // Clear and show
FastLED.delay(ms);           // Delay and handle timing
uint16_t fps = FastLED.getFPS();  // Get current frame rate
```

**Color Functions**:
```cpp
// Create colors
CRGB color = CRGB(r, g, b);         // RGB
CHSV color = CHSV(h, s, v);         // HSV
CRGB color = CRGB::Red;             // Named colors

// Blend colors
CRGB result = blend(color1, color2, amount);

// Fade colors
color.fadeToBlackBy(amount);        // 0-255
color.nscale8(scale);               // Scale brightness

// Add colors
leds[i] += CRGB(10, 0, 0);         // Add red
```

**Array Fill Functions**:
```cpp
fill_solid(leds, NUM_LEDS, color);
fill_rainbow(leds, NUM_LEDS, startHue, deltaHue);
fill_gradient_RGB(leds, NUM_LEDS, color1, color2);
fill_gradient_RGB(leds, NUM_LEDS, color1, color2, color3);
```

**Array Fade Functions**:
```cpp
fadeToBlackBy(leds, NUM_LEDS, amount);     // Fade toward black
fadeLightBy(leds, NUM_LEDS, amount);       // Same as fadeToBlackBy
nscale8(leds, NUM_LEDS, scale);            // Scale brightness
```

**Array Blur Functions**:
```cpp
blur1d(leds, NUM_LEDS, amount);            // Blur 1D array
```

**Math Functions**:
```cpp
// Scaling
uint8_t scale8(uint8_t i, uint8_t scale);              // i * scale / 255
uint16_t scale16(uint16_t i, uint16_t scale);          // 16-bit version

// Mapping
uint8_t map(value, fromLow, fromHigh, toLow, toHigh);

// Adding/Subtracting with saturation
uint8_t qadd8(uint8_t i, uint8_t j);                   // Add, max 255
uint8_t qsub8(uint8_t i, uint8_t j);                   // Subtract, min 0

// Sine waves
uint8_t sin8(uint8_t theta);                           // Fast 8-bit sine
uint16_t sin16(uint16_t theta);                        // 16-bit sine

// Beat functions
uint8_t beat8(uint16_t bpm);                           // Sawtooth
uint8_t beatsin8(uint16_t bpm, uint8_t min, uint8_t max);  // Sine wave
```

**Random Functions**:
```cpp
uint8_t random8();                     // 0-255
uint8_t random8(uint8_t max);         // 0 to max-1
uint8_t random8(uint8_t min, uint8_t max);
uint16_t random16();                   // 0-65535
uint16_t random16(uint16_t max);
```

**Noise Functions**:
```cpp
uint8_t inoise8(uint16_t x);
uint8_t inoise8(uint16_t x, uint16_t y);
uint8_t inoise8(uint16_t x, uint16_t y, uint16_t z);
uint16_t inoise16(uint32_t x, uint32_t y, uint32_t z);
```

**Timing Macros**:
```cpp
EVERY_N_MILLISECONDS(ms) { /* code */ }
EVERY_N_SECONDS(seconds) { /* code */ }

// Get current time
uint32_t now = millis();
```

**Color Palettes**:
```cpp
// Built-in palettes
RainbowColors_p
RainbowStripeColors_p
CloudColors_p
PartyColors_p
OceanColors_p
LavaColors_p
ForestColors_p
HeatColors_p

// Use palette
CRGB color = ColorFromPalette(palette, index, brightness, blendType);
// blendType: LINEARBLEND or NOBLEND
```

### Pin Configurations by Platform

**Arduino Uno/Nano (ATmega328)**:
```cpp
// Any digital pin can be used for data
#define DATA_PIN 6

// Recommended pins for best performance:
// Pins 2-13 (all digital pins work)
// Avoid pins 0,1 (Serial), 13 (LED)
```

**Arduino Mega (ATmega2560)**:
```cpp
#define DATA_PIN 6

// Many pins available: 2-53
// Best performance: pins 2-13, 44-46
```

**ESP32**:
```cpp
// Most GPIO pins work, but avoid:
// GPIO 0, 2 (boot pins)
// GPIO 6-11 (flash pins)
// GPIO 34-39 (input only)

// Recommended pins:
#define DATA_PIN 16  // or 17, 18, 19, 21, 22, 23

// For RMT-based output (best):
// GPIO 2, 4, 5, 12-19, 21-23, 25-27, 32-33

// Multiple strips example:
#define STRIP1_PIN 16
#define STRIP2_PIN 17
#define STRIP3_PIN 18
```

**ESP8266**:
```cpp
// Limited pins available
#define DATA_PIN 2   // GPIO2 (D4 on NodeMCU)

// Other options:
// GPIO0 (D3), GPIO2 (D4), GPIO4 (D2), GPIO5 (D1)
// GPIO12 (D6), GPIO13 (D7), GPIO14 (D5), GPIO15 (D8)

// Avoid during boot: GPIO0, GPIO2, GPIO15
```

**Teensy 4.x**:
```cpp
// Most pins work, excellent performance
#define DATA_PIN 7

// Recommended for WS2812B: Any pin
// Clock pins for APA102: 13, 14
```

**STM32 (BluePill)**:
```cpp
#define DATA_PIN PA7

// Available pins: Most GPIO pins
// SPI for APA102: PA5 (SCK), PA7 (MOSI)
```

**Raspberry Pi Pico (RP2040)**:
```cpp
#define DATA_PIN 16

// GPIO pins 0-29 available
// PIO-based output provides excellent performance
```

**Platform-Specific Notes**:

| Platform | Max LEDs | Notes |
|----------|----------|-------|
| Arduino Uno | ~500 | Limited by RAM (2KB) |
| Arduino Mega | ~2000 | More RAM (8KB) |
| ESP32 | 1000+ | Use PSRAM for more |
| ESP8266 | ~500 | Limited RAM |
| Teensy 4.0 | 5000+ | Very fast, lots of RAM |
| STM32 | 1000+ | Good performance |
| RP2040 | 1000+ | Excellent with PIO |

**Memory Calculation**:
```
Each LED uses 3 bytes (RGB)
Example: 500 LEDs = 1500 bytes

Arduino Uno: 2KB RAM total
- System: ~500 bytes
- Your code: ~500 bytes
- LEDs: ~1000 bytes (333 LEDs max)
```

### Supported LED Types

**Common LED Types**:

| LED Type | Clock Pin | Voltage | Speed | Notes |
|----------|-----------|---------|-------|-------|
| WS2812B | No | 5V | 800 KHz | Most popular, "NeoPixel" |
| SK6812 | No | 5V | 800 KHz | Similar to WS2812B, RGBW available |
| WS2813 | No | 5V | 800 KHz | Backup data line |
| APA102 | Yes | 5V | 1-20 MHz | "DotStar", faster refresh |
| SK9822 | Yes | 5V | 1-20 MHz | Similar to APA102 |
| WS2801 | Yes | 5V | 25 MHz | Older, requires clock |
| LPD8806 | Yes | 5V | 20 MHz | Older strip type |
| P9813 | Yes | 5V | 15 MHz | Common in modules |

**Code Examples by Type**:

**WS2812B / NeoPixel**:
```cpp
#define LED_TYPE WS2812B
#define COLOR_ORDER GRB
FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);
```

**APA102 / DotStar**:
```cpp
#define LED_TYPE APA102
#define COLOR_ORDER BGR
FastLED.addLeds<LED_TYPE, DATA_PIN, CLOCK_PIN, COLOR_ORDER>(leds, NUM_LEDS);
```

**SK6812 (RGBW)**:
```cpp
// Note: FastLED doesn't natively support RGBW, use workaround:
#define LED_TYPE SK6812
#define COLOR_ORDER GRB
FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);
```

**WS2813**:
```cpp
#define LED_TYPE WS2813
#define COLOR_ORDER GRB
FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);
```

**Color Order Reference**:
```
Common orders:
- GRB: WS2812B, WS2813, most strips
- RGB: Some WS2811, APA106
- BGR: APA102, SK9822
- BRG: Rare
```

**Testing Color Order**:
```cpp
void testColorOrder() {
    leds[0] = CRGB(255, 0, 0);  // Should show RED
    leds[1] = CRGB(0, 255, 0);  // Should show GREEN
    leds[2] = CRGB(0, 0, 255);  // Should show BLUE
    FastLED.show();

    // If wrong, try different COLOR_ORDER in setup
}
```

**Choosing LED Type**:

**Use WS2812B when:**
- Budget-friendly option needed
- Single data wire preferred
- Speed isn't critical
- Most common and compatible

**Use APA102 when:**
- Need higher refresh rates (>400 Hz)
- Want better color accuracy
- Have SPI pins available
- Need longer cable runs

---

**Additional Resources**:
- FastLED Documentation: https://fastled.io
- GitHub Repository: https://github.com/FastLED/FastLED
- Community Wiki: https://github.com/FastLED/FastLED/wiki

---

**Note**: This cookbook is a living document. Sections marked "Content to be added" will be expanded over time. Contributions welcome!
