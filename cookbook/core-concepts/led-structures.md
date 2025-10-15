# Understanding LED Data Structures

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

## Next Steps

- [Color Theory](color-theory.md) - Learn about RGB vs HSV
- [Basic Patterns](../basic-patterns/) - Use arrays to create patterns
