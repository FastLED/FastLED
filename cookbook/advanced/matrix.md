# 2D/Matrix Operations

**Difficulty Level**: ⭐⭐⭐ Advanced
**Time to Complete**: 45-60 minutes
**Prerequisites**:
- [Getting Started](../getting-started/) - Basic LED control and FastLED setup
- [Core Concepts](../core-concepts/) - Especially arrays, color theory, and timing
- [Basic Patterns](../basic-patterns/) - Pattern programming fundamentals
- [Intermediate Techniques](../intermediate/) - Noise functions, palettes, and math utilities

**You'll Learn**:
- How to map 2D coordinates (X, Y) to 1D LED array indices
- The difference between serpentine and progressive matrix layouts
- How to create 2D patterns like plasma effects, bouncing shapes, and heat maps
- Performance optimization techniques for matrix animations
- How to troubleshoot common matrix wiring and positioning issues

---

When working with LED matrices or grids, you need to map 2D coordinates to a 1D array. This guide covers the essential techniques for creating patterns on LED matrices.

## XY Mapping Function

The most important function for matrix work is the XY mapper, which converts 2D coordinates to array indices.

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

### Understanding Serpentine Layout

Most LED matrices are wired in a serpentine (zigzag) pattern:
- Row 0: Left to right (0, 1, 2, ...)
- Row 1: Right to left (..., 17, 16, 15)
- Row 2: Left to right (32, 33, 34, ...)

This layout minimizes wiring complexity but requires special handling in code.

## 2D Patterns

### Plasma Effect

Create organic, flowing patterns using 2D noise:

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

### Moving Shapes

Animate objects across the matrix:

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

## Tips for Matrix Programming

### Layout Types

**Serpentine (Zigzag)**:
- Most common wiring pattern
- Requires conditional logic in XY function
- Saves wire and simplifies physical layout

**Progressive (Rows all same direction)**:
- Simpler XY function: `return (y * MATRIX_WIDTH) + x`
- Requires more wiring
- Less common in commercial matrices

### Performance Considerations

1. **Pre-calculate constants**: Store matrix dimensions and frequently used values
2. **Use lookup tables**: For complex XY mappings, pre-compute and store indices
3. **Optimize nested loops**: Consider processing only changed pixels for complex effects
4. **Limit resolution**: For very large matrices, consider updating every other pixel

### Coordinate Systems

Remember that LED matrices can have different origin points:
- Bottom-left origin (most common)
- Top-left origin (displays)
- Center origin (some effects)

Adjust your XY function accordingly.

## Common Patterns

### Checkerboard
```cpp
void checkerboard() {
    for (uint8_t x = 0; x < MATRIX_WIDTH; x++) {
        for (uint8_t y = 0; y < MATRIX_HEIGHT; y++) {
            CRGB color = ((x + y) % 2) ? CRGB::White : CRGB::Black;
            leds[XY(x, y)] = color;
        }
    }
}
```

### Scrolling Text
For text display on matrices, consider using the LED-Matrix library in combination with FastLED.

### Heat Map
```cpp
void heatMap() {
    for (uint8_t x = 0; x < MATRIX_WIDTH; x++) {
        for (uint8_t y = 0; y < MATRIX_HEIGHT; y++) {
            // Distance from center
            int dx = x - MATRIX_WIDTH / 2;
            int dy = y - MATRIX_HEIGHT / 2;
            uint8_t distance = sqrt(dx * dx + dy * dy) * 10;

            leds[XY(x, y)] = ColorFromPalette(HeatColors_p, distance);
        }
    }
}
```

## Troubleshooting

**Wrong pixel positions**:
- Verify your matrix wiring pattern (serpentine vs progressive)
- Check if origin is bottom-left vs top-left
- Test with a simple pattern like checkerboard

**Performance issues**:
- Reduce matrix resolution
- Optimize nested loops
- Use EVERY_N_MILLISECONDS to limit update rate
- Consider FastLED.setMaxRefreshRate()

**Memory limitations**:
- Each LED uses 3 bytes (RGB)
- 16x16 matrix = 768 bytes
- Leave room for other variables and stack space

---

**Next Steps**:
- Try combining 2D patterns with color palettes
- Experiment with different noise scales for unique effects
- Add interactivity with sensors or controls
