# Fire Effect

**Difficulty Level**: ⭐⭐ Intermediate
**Time to Complete**: 30-40 minutes
**Prerequisites**:
- [Palettes](../intermediate/palettes.md) - Understanding ColorFromPalette() and built-in palettes
- [Math Functions](../intermediate/math.md) - Using scale8(), qadd8(), qsub8() for calculations
- [Arrays and LED Structures](../core-concepts/led-structures.md) - Working with parallel data arrays
- [Animations](../basic-patterns/animations.md) - Basic animation concepts and timing

**You'll Learn**:
- How to simulate natural phenomena using heat arrays and diffusion algorithms
- Mapping abstract data (heat values) to visual output (LED colors) with palettes
- Tuning parameters (cooling, sparking) to create different fire behaviors
- Using parallel arrays to store state data alongside LED color data
- Creating organic, realistic animations through randomness and physics-inspired rules

Realistic fire simulation using heat array and color palette.

## Basic Fire Effect

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

## Customizable Fire

Control the fire's behavior with adjustable parameters:

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

## How It Works

The fire effect uses a heat simulation approach:

1. **Cooling**: Each "cell" cools down randomly over time
2. **Heat diffusion**: Heat rises and spreads to neighboring cells
3. **Sparking**: New sparks are randomly ignited at the bottom
4. **Color mapping**: Heat values are mapped to colors using the HeatColors palette

## Parameters Explained

- **Cooling** (20-100): Controls how quickly the fire dies down
  - Lower values = slower, smoother fire
  - Higher values = faster, more chaotic fire

- **Sparking** (50-200): Controls how many new sparks are created
  - Lower values = sparse, gentle flames
  - Higher values = intense, vigorous fire

## Usage Tips

- Orient your LED strip vertically with the data input at the bottom for best visual effect
- For a campfire look, use default parameters: `customFire(55, 120)`
- For a gentle candle effect, try: `customFire(30, 80)`
- For an intense inferno, try: `customFire(80, 180)`
- Adjust `delay(15)` to control animation speed (lower = faster)

## Variations

**Blue/Green Fire:**
```cpp
// Use LavaColors_p or create custom palette for different fire colors
leds[j] = ColorFromPalette(OceanColors_p, scale8(heat[j], 240));
```

**Rainbow Fire:**
```cpp
// Map heat to hue for psychedelic fire
leds[j] = CHSV(scale8(heat[j], 255), 255, heat[j]);
```
