# Timing and Frame Rate

**Difficulty Level**: ⭐⭐ Beginner to Intermediate
**Time to Complete**: 35-45 minutes (reading and experimenting with different timing approaches)
**Prerequisites**:
- [Getting Started: Basic Concepts](../getting-started/concepts.md) - Understanding FastLED fundamentals
- [Getting Started: First Example](../getting-started/first-example.md) - Hands-on programming experience

**You'll Learn**:
- How to control animation speed using multiple timing methods (delay, EVERY_N, millis)
- The difference between blocking and non-blocking timing approaches
- How to calculate and measure frame rates for smooth animations
- Advanced timing techniques including beat synchronization and time-based effects
- Best practices for performance optimization and timing reliability

Control animation speed and timing in FastLED.

## Simple Delay

The simplest timing method using `delay()`:

```cpp
void loop() {
    updateLEDs();
    FastLED.show();
    delay(20);  // ~50 FPS (20ms between frames)
}
```

**Pros**: Simple, easy to understand
**Cons**: Blocks code execution - nothing else can run during delay

## Frame Rate Calculation

```
Frame rate (FPS) = 1000ms / delay_time

Examples:
- delay(20)  = 50 FPS
- delay(10)  = 100 FPS
- delay(50)  = 20 FPS
- delay(100) = 10 FPS
```

## EVERY_N_MILLISECONDS Macro

Non-blocking timing using FastLED's built-in macro:

```cpp
void loop() {
    EVERY_N_MILLISECONDS(20) {  // 50 FPS
        updateLEDs();
        FastLED.show();
    }

    // Other code can run here
    checkButtons();
    readSensors();
}
```

**Pros**: Non-blocking, other code can run
**Cons**: Fixed interval, less precise than custom timing

## EVERY_N_SECONDS Macro

For slower updates:

```cpp
void loop() {
    EVERY_N_SECONDS(1) {
        Serial.println("One second elapsed");
    }

    EVERY_N_MILLISECONDS(20) {
        updateLEDs();
        FastLED.show();
    }
}
```

## Custom Non-Blocking Timing

Full control using `millis()`:

```cpp
uint32_t lastUpdate = 0;
const uint32_t updateInterval = 20;  // 20ms = 50 FPS

void loop() {
    uint32_t now = millis();

    if (now - lastUpdate >= updateInterval) {
        lastUpdate = now;

        updateLEDs();
        FastLED.show();
    }

    // Other code runs every loop
    checkButtons();
}
```

**Pros**: Non-blocking, precise control
**Cons**: More code to manage

## Multiple Timing Intervals

Run different effects at different speeds:

```cpp
uint32_t lastLEDUpdate = 0;
uint32_t lastSerialUpdate = 0;

const uint32_t LED_INTERVAL = 20;     // 50 FPS
const uint32_t SERIAL_INTERVAL = 1000; // 1 Hz

void loop() {
    uint32_t now = millis();

    // Update LEDs at 50 FPS
    if (now - lastLEDUpdate >= LED_INTERVAL) {
        lastLEDUpdate = now;
        updateLEDs();
        FastLED.show();
    }

    // Print status at 1 Hz
    if (now - lastSerialUpdate >= SERIAL_INTERVAL) {
        lastSerialUpdate = now;
        Serial.println("Status update");
    }
}
```

## FastLED.delay()

FastLED provides its own delay function with built-in dithering:

```cpp
void loop() {
    updateLEDs();
    FastLED.show();
    FastLED.delay(20);  // Like delay() but maintains dithering
}
```

Use `FastLED.delay()` instead of `delay()` for better color rendering.

## Measuring Frame Rate

Track actual FPS:

```cpp
void loop() {
    updateLEDs();
    FastLED.show();

    EVERY_N_SECONDS(1) {
        uint16_t fps = FastLED.getFPS();
        Serial.print("FPS: ");
        Serial.println(fps);
    }

    delay(20);
}
```

## Variable Speed Effects

Make animation speed configurable:

```cpp
uint8_t animationSpeed = 10;  // Adjustable speed

void loop() {
    EVERY_N_MILLISECONDS(20) {
        static uint8_t hue = 0;

        fill_rainbow(leds, NUM_LEDS, hue, 7);
        FastLED.show();

        hue += animationSpeed;  // Speed controls how fast hue changes
    }
}
```

## Time-Based Animation

Use `millis()` directly for smooth effects:

```cpp
void loop() {
    uint32_t now = millis();

    for (int i = 0; i < NUM_LEDS; i++) {
        // Create time-based wave
        uint8_t brightness = sin8(now / 10 + i * 20);
        leds[i] = CHSV(160, 255, brightness);
    }

    FastLED.show();
}
```

## Beat-Synchronized Effects

Use FastLED's beat functions:

```cpp
void loop() {
    // Sawtooth wave (0-255 repeating)
    uint8_t beat = beat8(60);  // 60 BPM

    // Sine wave (0-255 oscillating)
    uint8_t sineBeat = beatsin8(60, 0, 255);

    // Apply to LEDs
    fill_solid(leds, NUM_LEDS, CHSV(beat, 255, 255));
    FastLED.show();
}
```

## Timing Challenges

### millis() Overflow

`millis()` overflows after ~49 days. Handle gracefully:

```cpp
// GOOD: Subtraction handles overflow correctly
uint32_t now = millis();
if (now - lastUpdate >= interval) {
    // This works even after overflow
}

// BAD: Direct comparison fails after overflow
if (millis() >= lastUpdate + interval) {
    // Don't use this pattern
}
```

### Interrupt Interference

Some operations can interfere with LED timing:

```cpp
void loop() {
    // Disable interrupts during critical LED update
    noInterrupts();
    FastLED.show();
    interrupts();

    // Other code
}
```

On ESP32, WiFi can interfere. Consider disabling WiFi during `FastLED.show()`.

## Performance Optimization

### Limit Update Rate

Don't update faster than necessary:

```cpp
// WASTEFUL: 1000+ FPS (updates too fast)
void loop() {
    updateLEDs();
    FastLED.show();  // No delay
}

// BETTER: 50 FPS is plenty
void loop() {
    EVERY_N_MILLISECONDS(20) {
        updateLEDs();
        FastLED.show();
    }
}
```

### Separate Calculation from Display

```cpp
void loop() {
    EVERY_N_MILLISECONDS(20) {
        // Only call show() when needed
        FastLED.show();
    }

    // Calculate every loop (faster)
    updateLEDs();
}
```

## Practical Examples

### Smooth Fade In/Out

```cpp
void fadeInOut() {
    static uint8_t brightness = 0;
    static int8_t direction = 1;

    EVERY_N_MILLISECONDS(10) {
        brightness += direction;

        if (brightness == 0 || brightness == 255) {
            direction = -direction;
        }

        FastLED.setBrightness(brightness);
        fill_solid(leds, NUM_LEDS, CRGB::Purple);
        FastLED.show();
    }
}
```

### Speed-Controlled Chase

```cpp
uint8_t chaseSpeed = 50;  // ms between steps

void chase() {
    static uint8_t pos = 0;
    static uint32_t lastMove = 0;

    uint32_t now = millis();
    if (now - lastMove >= chaseSpeed) {
        lastMove = now;

        FastLED.clear();
        leds[pos] = CRGB::Red;
        FastLED.show();

        pos = (pos + 1) % NUM_LEDS;
    }
}
```

### Multi-Speed Effect

```cpp
void multiSpeed() {
    static uint8_t hue1 = 0;
    static uint8_t hue2 = 128;

    // Fast effect (every 10ms)
    EVERY_N_MILLISECONDS(10) {
        leds[0] = CHSV(hue1, 255, 255);
        hue1 += 2;
    }

    // Slow effect (every 50ms)
    EVERY_N_MILLISECONDS(50) {
        leds[NUM_LEDS - 1] = CHSV(hue2, 255, 255);
        hue2++;
    }

    // Update display at fixed rate
    EVERY_N_MILLISECONDS(20) {
        FastLED.show();
    }
}
```

## Best Practices

1. **Use non-blocking timing** for responsive code
2. **Aim for 30-60 FPS** - higher is usually unnecessary
3. **Separate calculation from display** when possible
4. **Use FastLED.delay()** instead of delay() for better dithering
5. **Monitor FPS** during development to catch performance issues

## Next Steps

- [Power Considerations](power.md) - Manage power consumption
- [Basic Patterns](../basic-patterns/) - Apply timing to animations
