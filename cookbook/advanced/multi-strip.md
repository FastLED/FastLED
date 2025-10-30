# Multiple Strip Coordination

**Difficulty Level**: ⭐⭐⭐ Advanced
**Time to Complete**: 40-50 minutes
**Prerequisites**:
- [Getting Started](../getting-started/) - Basic LED control and FastLED setup
- [Core Concepts](../core-concepts/) - Especially arrays and timing
- [Basic Patterns](../basic-patterns/) - Pattern programming and animations
- [Intermediate Techniques](../intermediate/) - Math utilities and coordination patterns

**You'll Learn**:
1. How to control multiple LED strips from a single microcontroller
2. Techniques for synchronizing effects across multiple strips
3. How to create wave and chase effects that span multiple strips
4. The virtual combined strip concept for treating multiple strips as one
5. Performance considerations, memory usage, and troubleshooting for multi-strip setups

---

Control multiple LED strips independently or in coordination for larger installations and complex effects.

## Setting Up Multiple Controllers

FastLED makes it easy to control multiple LED strips from a single microcontroller.

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

### Multiple Strip Arrays

You can also use an array of arrays for easier management:

```cpp
#define NUM_STRIPS 4
#define LEDS_PER_STRIP 60

CRGB strips[NUM_STRIPS][LEDS_PER_STRIP];

void setup() {
    FastLED.addLeds<WS2812B, 5, GRB>(strips[0], LEDS_PER_STRIP);
    FastLED.addLeds<WS2812B, 6, GRB>(strips[1], LEDS_PER_STRIP);
    FastLED.addLeds<WS2812B, 7, GRB>(strips[2], LEDS_PER_STRIP);
    FastLED.addLeds<WS2812B, 8, GRB>(strips[3], LEDS_PER_STRIP);
}
```

## Synchronized Effects

Apply the same pattern to all strips simultaneously.

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

### Using Loops for Synchronization

```cpp
void syncedPattern() {
    static uint8_t hue = 0;

    for (int s = 0; s < NUM_STRIPS; s++) {
        fill_rainbow(strips[s], LEDS_PER_STRIP, hue, 5);
    }

    hue++;
}
```

## Independent Control

Each strip runs a different effect.

```cpp
void independentEffects() {
    // Strip 1: Rainbow
    static uint8_t hue1 = 0;
    fill_rainbow(strip1, LEDS_PER_STRIP, hue1, 5);
    hue1++;

    // Strip 2: Solid color breathing
    uint8_t brightness = beatsin8(30);
    fill_solid(strip2, LEDS_PER_STRIP, CHSV(160, 255, brightness));

    // Strip 3: Scanner/Cylon
    static uint8_t pos = 0;
    fadeToBlackBy(strip3, LEDS_PER_STRIP, 20);
    strip3[pos] = CRGB::Red;
    pos = (pos + 1) % LEDS_PER_STRIP;
}
```

## Wave Across Strips

Create effects that move from one strip to another.

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

### Generalized Wave Function

```cpp
void generalizedWave() {
    static uint8_t phase = 0;

    for (int s = 0; s < NUM_STRIPS; s++) {
        // Create wave with phase offset per strip
        for (int i = 0; i < LEDS_PER_STRIP; i++) {
            uint8_t brightness = sin8(phase + (s * 40) + (i * 5));
            strips[s][i] = CHSV(160, 255, brightness);
        }
    }

    phase += 2;
}
```

## Coordinated Patterns

### Alternating Strips

```cpp
void alternatingStrips() {
    static uint8_t state = 0;
    EVERY_N_MILLISECONDS(500) {
        state = !state;
    }

    if (state) {
        fill_solid(strip1, LEDS_PER_STRIP, CRGB::Red);
        fill_solid(strip2, LEDS_PER_STRIP, CRGB::Black);
        fill_solid(strip3, LEDS_PER_STRIP, CRGB::Red);
    } else {
        fill_solid(strip1, LEDS_PER_STRIP, CRGB::Black);
        fill_solid(strip2, LEDS_PER_STRIP, CRGB::Blue);
        fill_solid(strip3, LEDS_PER_STRIP, CRGB::Black);
    }
}
```

### Chase Across Strips

```cpp
void chaseAcrossStrips() {
    static uint8_t stripIndex = 0;
    static uint8_t ledIndex = 0;

    // Fade all
    fadeToBlackBy(strip1, LEDS_PER_STRIP, 50);
    fadeToBlackBy(strip2, LEDS_PER_STRIP, 50);
    fadeToBlackBy(strip3, LEDS_PER_STRIP, 50);

    // Light current position
    switch(stripIndex) {
        case 0: strip1[ledIndex] = CRGB::White; break;
        case 1: strip2[ledIndex] = CRGB::White; break;
        case 2: strip3[ledIndex] = CRGB::White; break;
    }

    // Advance position
    ledIndex++;
    if (ledIndex >= LEDS_PER_STRIP) {
        ledIndex = 0;
        stripIndex = (stripIndex + 1) % NUM_STRIPS;
    }
}
```

## Virtual Combined Strip

Treat multiple strips as one continuous strip.

```cpp
// Helper function to set LED on virtual strip
void setVirtualLED(uint16_t index, CRGB color) {
    uint8_t stripNum = index / LEDS_PER_STRIP;
    uint8_t ledNum = index % LEDS_PER_STRIP;

    if (stripNum < NUM_STRIPS) {
        strips[stripNum][ledNum] = color;
    }
}

void virtualStripEffect() {
    uint16_t totalLEDs = NUM_STRIPS * LEDS_PER_STRIP;

    // Treat as single rainbow
    for (uint16_t i = 0; i < totalLEDs; i++) {
        uint8_t hue = map(i, 0, totalLEDs, 0, 255);
        setVirtualLED(i, CHSV(hue, 255, 255));
    }
}
```

## Performance Considerations

### Update All Strips Together

```cpp
void loop() {
    updateAllStrips();
    FastLED.show();  // Updates all controllers at once
    delay(20);
}
```

FastLED.show() updates all registered controllers efficiently, so you don't need to call it separately for each strip.

### Memory Usage

Each LED uses 3 bytes (RGB). Plan accordingly:
- 3 strips × 60 LEDs = 180 LEDs = 540 bytes
- 5 strips × 100 LEDs = 500 LEDs = 1,500 bytes

Make sure your microcontroller has sufficient RAM.

### Pin Selection

Choose pins that support hardware features on your platform:
- ESP32: Use RMT-capable pins for best performance
- Arduino: Any digital pin works
- Teensy: Any pin, excellent parallel output
- ESP8266: Limited pins, use GPIO carefully

## Troubleshooting

**One strip doesn't work**:
- Verify pin assignment in addLeds()
- Check power connections to that strip
- Test the strip independently
- Verify color order matches strip type

**Strips flicker at different rates**:
- Ensure FastLED.show() is called once per frame
- Check that all strips use the same frame rate
- Verify power supply capacity

**Memory issues**:
- Calculate total LED memory: NUM_STRIPS × LEDS_PER_STRIP × 3 bytes
- Consider reducing strip count or length
- Use dynamic allocation cautiously

**Timing issues with many strips**:
- Some platforms can update all strips in parallel (ESP32, Teensy)
- Others update serially (Arduino Uno)
- Adjust frame rate if updates take too long

## Advanced Techniques

### Strip Groups

Create groups of strips for easier management:

```cpp
void updateStripGroup(int startStrip, int endStrip, CRGB color) {
    for (int s = startStrip; s <= endStrip; s++) {
        fill_solid(strips[s], LEDS_PER_STRIP, color);
    }
}

void groupedEffect() {
    updateStripGroup(0, 1, CRGB::Red);    // First two strips
    updateStripGroup(2, 3, CRGB::Blue);   // Next two strips
}
```

### Mirror Effects

Mirror one strip onto another:

```cpp
void mirrorStrips() {
    // Copy strip1 to strip2 in reverse
    for (int i = 0; i < LEDS_PER_STRIP; i++) {
        strip2[i] = strip1[LEDS_PER_STRIP - 1 - i];
    }
}
```

---

**Next Steps**:
- Experiment with phase offsets between strips
- Try different coordination patterns (circular, radial, etc.)
- Combine with audio reactivity for immersive effects
- Create installations with strips at different angles or distances
