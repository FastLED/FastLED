# Performance Optimization

**Difficulty Level**: ⭐⭐⭐ Advanced
**Time to Complete**: 50-60 minutes
**Prerequisites**:
- [Getting Started](../getting-started/) - Basic LED control and FastLED setup
- [Core Concepts](../core-concepts/) - Especially timing and memory considerations
- [Basic Patterns](../basic-patterns/) - Pattern programming fundamentals
- [Intermediate Techniques](../intermediate/) - Advanced patterns that benefit from optimization

**You'll Learn**:
1. How to identify and eliminate performance bottlenecks in LED code
2. Techniques for optimizing calculations (integer math, precalculation, bit shifts)
3. Memory optimization strategies for constrained microcontrollers
4. Frame rate management and power consumption control
5. Platform-specific optimization tips and profiling techniques

---

Tips and techniques for creating smooth, efficient LED animations. Optimizing your code ensures high frame rates and responsive effects.

## Reduce Calculations in Loop

Avoid recalculating constants and expensive operations every frame.

### Bad: Recalculating Every Frame

```cpp
void slowEffect() {
    for (int i = 0; i < NUM_LEDS; i++) {
        float angle = (i * 360.0) / NUM_LEDS;  // Expensive floating point math!
        float radians = angle * PI / 180.0;
        // ... more calculations
    }
}
```

### Good: Precalculate Once

```cpp
uint8_t ledAngles[NUM_LEDS];

void setup() {
    // Calculate once at startup
    for (int i = 0; i < NUM_LEDS; i++) {
        ledAngles[i] = (i * 255) / NUM_LEDS;
    }

    FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);
}

void fastEffect() {
    for (int i = 0; i < NUM_LEDS; i++) {
        // Use precalculated value
        uint8_t brightness = sin8(ledAngles[i] + millis() / 10);
        leds[i] = CHSV(160, 255, brightness);
    }
}
```

## Use Integer Math

Floating-point operations are slow on microcontrollers. Use integer math and bit shifts when possible.

### Float Division (Slow)

```cpp
float result = (value * 3.14159) / 2.0;
```

### Integer Operations (Fast)

```cpp
// Bit shift for division by powers of 2
uint8_t result = (value * 3) >> 1;  // Divide by 2 using bit shift

// Use FastLED's scale8 for scaling
uint8_t scaled = scale8(value, 128);  // Multiply by 128/255

// Integer approximations
uint8_t approxPi = (value * 157) >> 5;  // Approximate × π (157/50 ≈ π)
```

## Limit Frame Rate

Don't update LEDs faster than necessary. Most effects look smooth at 30-60 FPS.

### Simple Rate Limiting

```cpp
void loop() {
    EVERY_N_MILLISECONDS(20) {  // Max 50 FPS
        updateLEDs();
        FastLED.show();
    }
    // Other non-blocking tasks can run here
}
```

### Manual Timing Control

```cpp
const uint32_t FRAME_TIME = 20;  // 50 FPS
uint32_t lastFrame = 0;

void loop() {
    uint32_t now = millis();

    if (now - lastFrame >= FRAME_TIME) {
        lastFrame = now;
        updateLEDs();
        FastLED.show();
    }

    // Other tasks
}
```

### Adaptive Frame Rate

```cpp
void loop() {
    updateLEDs();
    FastLED.show();

    // Monitor and adjust
    EVERY_N_SECONDS(5) {
        uint16_t fps = FastLED.getFPS();
        if (fps < 30) {
            Serial.println("WARNING: Low frame rate detected!");
        }
    }
}
```

## Power Management

Limit power consumption to stay within your power supply's capacity.

```cpp
void setup() {
    FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);

    // Limit total power draw
    FastLED.setMaxPowerInVoltsAndMilliamps(5, 2000);  // 5V, 2A max
}
```

This automatically scales brightness to prevent exceeding your power budget.

### Manual Brightness Limiting

```cpp
void setup() {
    FastLED.setBrightness(50);  // Global brightness limit (0-255)
}

// Or per-effect:
void conservativeBrightness() {
    for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = CHSV(hue, 255, 128);  // Max 50% brightness
    }
}
```

## Efficient Array Operations

Use FastLED's optimized functions instead of manual loops.

### Slow: Individual Assignments

```cpp
for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CRGB::Black;
}
```

### Fast: FastLED Functions

```cpp
fill_solid(leds, NUM_LEDS, CRGB::Black);
// Or even faster:
FastLED.clear();
```

### Other Efficient Functions

```cpp
// Fill with rainbow
fill_rainbow(leds, NUM_LEDS, startHue, deltaHue);

// Fade all LEDs
fadeToBlackBy(leds, NUM_LEDS, fadeAmount);

// Blur for smoothing
blur1d(leds, NUM_LEDS, blurAmount);
```

## Temporal Dithering

Dithering reduces visible banding in color gradients but costs performance.

```cpp
void setup() {
    FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);

    // Balance quality vs speed
    FastLED.setDither(BINARY_DITHER);  // Better quality (default)
    // FastLED.setDither(DISABLE_DITHER);  // Faster, slight quality loss
}
```

For most effects, BINARY_DITHER is fine. Disable only if you need maximum frame rate.

## Memory Optimization

### Reduce LED Count

Each LED uses 3 bytes of RAM:
- 100 LEDs = 300 bytes
- 500 LEDs = 1,500 bytes
- 1000 LEDs = 3,000 bytes

On memory-constrained devices (Arduino Uno: 2KB RAM), consider:
- Using fewer LEDs
- Splitting across multiple controllers
- Upgrading to a device with more RAM (ESP32, Teensy)

### Avoid Dynamic Allocation

```cpp
// Bad: Dynamic allocation in loop
void inefficient() {
    CRGB* tempBuffer = new CRGB[NUM_LEDS];  // Slow, fragments memory
    // ...
    delete[] tempBuffer;
}

// Good: Static allocation
CRGB tempBuffer[NUM_LEDS];

void efficient() {
    // Use static buffer
}
```

### Use Smaller Data Types

```cpp
// If you only need 0-255, use uint8_t
uint8_t brightness = 128;

// Not:
int brightness = 128;  // Uses 2 bytes instead of 1
```

## Pattern-Specific Optimizations

### Optimize Noise Functions

```cpp
// Slower: Recalculate noise for every LED every frame
void slowNoise() {
    for (int i = 0; i < NUM_LEDS; i++) {
        uint8_t noise = inoise8(i * 100, i * 100, millis());
        leds[i] = CHSV(noise, 255, 255);
    }
}

// Faster: Use simpler parameters
void fastNoise() {
    uint16_t time = millis() / 10;
    for (int i = 0; i < NUM_LEDS; i++) {
        uint8_t noise = inoise8(i * 50, time);  // Fewer calculations
        leds[i] = CHSV(noise, 255, 255);
    }
}
```

### Optimize Palette Lookups

```cpp
// Precalculate palette index offsets
uint8_t paletteOffsets[NUM_LEDS];

void setup() {
    for (int i = 0; i < NUM_LEDS; i++) {
        paletteOffsets[i] = (i * 256) / NUM_LEDS;
    }
}

void paletteEffect() {
    static uint8_t baseIndex = 0;

    for (int i = 0; i < NUM_LEDS; i++) {
        uint8_t index = baseIndex + paletteOffsets[i];
        leds[i] = ColorFromPalette(palette, index, 255, LINEARBLEND);
    }

    baseIndex++;
}
```

## Platform-Specific Tips

### Arduino Uno/Nano (ATmega328)
- Very limited RAM (2KB)
- Slow CPU (16 MHz)
- Keep LED count under 500
- Avoid floating-point math
- Use PROGMEM for constant data

### ESP32
- Plenty of RAM (520KB)
- Fast CPU (240 MHz)
- Use RMT peripheral for best results
- Can handle 1000+ LEDs easily
- Disable WiFi during LED updates for smoother output

### ESP8266
- Limited RAM (80KB available)
- Moderate CPU (80/160 MHz)
- Keep LED count reasonable (500-1000)
- WiFi can cause LED glitches

### Teensy 4.x
- Excellent performance
- Very fast CPU (600 MHz)
- Can drive many LEDs with ease
- Parallel output capability

## Profiling and Debugging

### Measure Frame Rate

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

### Time Specific Functions

```cpp
void profileFunction() {
    uint32_t startTime = micros();

    // Function to profile
    updateComplexEffect();

    uint32_t duration = micros() - startTime;
    Serial.print("Effect took: ");
    Serial.print(duration);
    Serial.println(" microseconds");
}
```

### Identify Bottlenecks

```cpp
void findBottleneck() {
    uint32_t t1 = micros();
    part1();
    uint32_t t2 = micros();
    part2();
    uint32_t t3 = micros();
    part3();
    uint32_t t4 = micros();

    Serial.print("Part 1: "); Serial.println(t2 - t1);
    Serial.print("Part 2: "); Serial.println(t3 - t2);
    Serial.print("Part 3: "); Serial.println(t4 - t3);
}
```

## Best Practices Summary

1. **Precalculate constants** - Don't recalculate the same values every frame
2. **Use integer math** - Avoid floating-point operations on microcontrollers
3. **Limit frame rate** - 30-60 FPS is usually sufficient
4. **Use FastLED functions** - They're optimized for performance
5. **Manage power** - Use setMaxPowerInVoltsAndMilliamps()
6. **Profile your code** - Measure to find actual bottlenecks
7. **Choose appropriate hardware** - Match your needs to the platform's capabilities
8. **Test incrementally** - Add complexity gradually and test performance

## Common Performance Pitfalls

### Pitfall 1: Too Many Serial Prints
```cpp
// BAD: Serial in tight loop
for (int i = 0; i < NUM_LEDS; i++) {
    Serial.println(i);  // Very slow!
    leds[i] = color;
}

// GOOD: Print occasionally
EVERY_N_SECONDS(1) {
    Serial.println("Status: OK");
}
```

### Pitfall 2: Blocking Delays
```cpp
// BAD: Blocking delay
void loop() {
    updateLEDs();
    FastLED.show();
    delay(1000);  // Nothing can happen during delay
}

// GOOD: Non-blocking timing
EVERY_N_MILLISECONDS(1000) {
    updateLEDs();
}
```

### Pitfall 3: Unnecessary Full Strip Updates
```cpp
// If only a few LEDs change, don't recalculate all
// Instead, fade existing and add new:
fadeToBlackBy(leds, NUM_LEDS, 10);
leds[newPos] = CRGB::White;  // Only set changed LED
```

---

**Next Steps**:
- Profile your effects to find actual bottlenecks
- Experiment with different frame rates for your installation
- Test on your target hardware platform
- Balance quality vs performance for your specific needs
