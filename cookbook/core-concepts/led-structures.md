# Understanding LED Data Structures

**Difficulty Level**: ⭐⭐ Beginner to Intermediate
**Time to Complete**: 30-40 minutes
**Prerequisites**:
- [Basic Concepts](../getting-started/concepts.md) - Understanding of FastLED fundamentals
- [First Example](../getting-started/first-example.md) - Basic LED programming experience

**You'll Learn**:
- How to work with CRGB arrays and access individual LEDs
- Multiple ways to create and manipulate LED colors
- How to perform color arithmetic and blending operations
- Memory considerations for LED arrays on different platforms
- Classical LED programming techniques like pixel drawing and scanning patterns

How LED arrays work in FastLED.

## The CRGB Array

The fundamental data structure in FastLED is the `CRGB` array:

```cpp
CRGB leds[NUM_LEDS];  // Array of RGB color values
```

This creates an array where each element represents one LED's color.

## Accessing Individual LEDs

Each LED has an index starting from 0:

```cpp
leds[0] = CRGB::Red;           // First LED
leds[5] = CRGB(255, 0, 128);   // Direct RGB values
leds[10].r = 200;              // Access red channel directly
```

## CRGB Structure

A `CRGB` object contains three color channels:

```cpp
CRGB color;
color.r = 255;  // Red channel (0-255)
color.g = 128;  // Green channel (0-255)
color.b = 64;   // Blue channel (0-255)
```

## Creating Colors

### Named Colors

```cpp
leds[0] = CRGB::Red;
leds[1] = CRGB::Green;
leds[2] = CRGB::Blue;
leds[3] = CRGB::White;
leds[4] = CRGB::Black;  // Off
```

### Direct RGB Values

```cpp
leds[0] = CRGB(255, 0, 0);      // Red
leds[1] = CRGB(0, 255, 0);      // Green
leds[2] = CRGB(0, 0, 255);      // Blue
leds[3] = CRGB(255, 128, 64);   // Custom color
```

### HSV Colors (Automatic Conversion)

```cpp
leds[0] = CHSV(0, 255, 255);    // Red in HSV
leds[1] = CHSV(96, 255, 255);   // Green in HSV
```

## Manipulating Color Channels

### Direct Access

```cpp
CRGB color = CRGB::Purple;
color.r += 50;    // Increase red
color.g /= 2;     // Decrease green
color.b = 0;      // Remove blue
```

### Array Operations

```cpp
// Set range of LEDs
for (int i = 0; i < 10; i++) {
    leds[i] = CRGB::Blue;
}

// Modify existing colors
for (int i = 0; i < NUM_LEDS; i++) {
    leds[i].r += 10;  // Add red to all LEDs
}
```

## Color Arithmetic

CRGB supports mathematical operations:

```cpp
CRGB color1 = CRGB(100, 50, 25);
CRGB color2 = CRGB(50, 100, 25);

// Addition
CRGB result = color1 + color2;  // (150, 150, 50)

// Scaling
color1 *= 2;  // Double all channels (200, 100, 50)
color1 /= 2;  // Halve all channels (50, 25, 12)

// Modifying individual LEDs
leds[0] += CRGB(10, 0, 0);  // Add red
```

## Memory Considerations

Each LED uses 3 bytes of RAM:

```
Number of LEDs × 3 bytes = RAM used

Examples:
- 60 LEDs = 180 bytes
- 100 LEDs = 300 bytes
- 500 LEDs = 1500 bytes
```

**Arduino Uno** has only 2KB RAM total, limiting you to ~300-400 LEDs (accounting for program overhead).

## Working with Ranges

### Fill a Subset

```cpp
// Fill LEDs 10-19 with green
fill_solid(&leds[10], 10, CRGB::Green);

// Using pointer arithmetic
CRGB* startPos = &leds[20];
fill_solid(startPos, 15, CRGB::Blue);  // LEDs 20-34
```

### Copy Ranges

```cpp
// Copy pattern from one section to another
for (int i = 0; i < 10; i++) {
    leds[i + 20] = leds[i];  // Copy LEDs 0-9 to 20-29
}
```

## Best Practices

### Use fill_solid for Efficiency

```cpp
// GOOD: Use FastLED function
fill_solid(leds, NUM_LEDS, CRGB::Black);

// LESS EFFICIENT: Manual loop
for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CRGB::Black;
}
```

### Clear All LEDs

```cpp
// Option 1: fill_solid
fill_solid(leds, NUM_LEDS, CRGB::Black);

// Option 2: FastLED.clear (even faster)
FastLED.clear();  // Sets all to black, doesn't call show()
```

### Check Array Bounds

```cpp
int pos = calculatePosition();  // Some calculation

// Always validate before accessing
if (pos >= 0 && pos < NUM_LEDS) {
    leds[pos] = CRGB::Red;
}
```

## Array Manipulation

### Classical LED Drawing Techniques

FastLED builds on classical LED programming techniques where you directly manipulate pixel arrays. Understanding these fundamentals is essential for creating any LED effect.

#### Direct Pixel Addressing

The most basic form of LED control - setting individual pixels:

```cpp
void drawPixel(int position, CRGB color) {
    if (position >= 0 && position < NUM_LEDS) {
        leds[position] = color;
    }
}

// Usage
drawPixel(10, CRGB::Red);
drawPixel(15, CRGB(0, 255, 128));
```

#### Drawing Lines

Create lines by setting consecutive LEDs:

```cpp
void drawLine(int start, int end, CRGB color) {
    if (start > end) {
        int temp = start;
        start = end;
        end = temp;
    }

    for (int i = start; i <= end && i < NUM_LEDS; i++) {
        leds[i] = color;
    }
}

// Draw a red line from LED 5 to LED 20
drawLine(5, 20, CRGB::Red);
```

#### XY Coordinate Mapping for Matrices

For 2D LED matrices, convert X,Y coordinates to array indices:

```cpp
#define MATRIX_WIDTH 16
#define MATRIX_HEIGHT 16

uint16_t XY(uint8_t x, uint8_t y) {
    if (x >= MATRIX_WIDTH || y >= MATRIX_HEIGHT) return 0;

    // Serpentine (zigzag) layout - most common wiring pattern
    if (y & 0x01) {
        // Odd rows run backwards
        return (y * MATRIX_WIDTH) + (MATRIX_WIDTH - 1 - x);
    } else {
        // Even rows run forwards
        return (y * MATRIX_WIDTH) + x;
    }
}

// Draw a pixel at X=5, Y=10
leds[XY(5, 10)] = CRGB::Blue;

// Draw a horizontal line
for (int x = 0; x < MATRIX_WIDTH; x++) {
    leds[XY(x, 8)] = CRGB::Green;
}
```

#### Blending and Layering

Combine colors rather than replacing them:

```cpp
// Additive blending (brightens)
leds[i] += CRGB(20, 20, 20);

// Averaging (mix colors)
CRGB color1 = leds[i];
CRGB color2 = CRGB::Blue;
leds[i] = blend(color1, color2, 128);  // 50% blend
```

#### Classical Pattern: Scanning/Larson Scanner

The classic "Cylon eye" or "KITT" effect:

```cpp
void larsonScanner() {
    static int pos = 0;
    static int direction = 1;

    // Fade all LEDs
    fadeToBlackBy(leds, NUM_LEDS, 20);

    // Draw the scanner
    leds[pos] = CRGB::Red;

    // Move position
    pos += direction;
    if (pos >= NUM_LEDS - 1 || pos <= 0) {
        direction = -direction;
    }
}
```

These classical techniques form the foundation of all LED programming - from simple strips to complex 2D and 3D installations. See [2D/Matrix Operations](../advanced/matrix.md) for more advanced spatial mapping techniques.

## Next Steps

- [Color Theory](color-theory.md) - Learn about RGB vs HSV
- [Basic Patterns](../basic-patterns/) - Use arrays to create patterns
- [2D/Matrix Operations](../advanced/matrix.md) - Advanced XY mapping and matrix effects
