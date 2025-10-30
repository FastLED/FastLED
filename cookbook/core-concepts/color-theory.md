# Color Theory

Understanding RGB and HSV color models in FastLED.

**Difficulty Level**: ⭐⭐ Beginner to Intermediate
**Time to Complete**: 25-30 minutes (reading and experimenting with examples)
**Prerequisites**: [Getting Started](../getting-started/) - Basic FastLED setup and first example

**You'll Learn**:
- How to create colors using RGB (Red, Green, Blue) values
- How to use HSV (Hue, Saturation, Value) for easier color animations
- When to choose RGB vs HSV for different effects
- How to blend colors and adjust brightness programmatically
- How to convert between color models in FastLED

## RGB Color Model

RGB (Red, Green, Blue) uses three channels, each ranging from 0-255.

### Creating RGB Colors

```cpp
CRGB color1 = CRGB(255, 0, 0);      // Pure red
CRGB color2 = CRGB(0, 255, 0);      // Pure green
CRGB color3 = CRGB(0, 0, 255);      // Pure blue
CRGB color4 = CRGB(255, 255, 255);  // White
CRGB color5 = CRGB(128, 128, 128);  // Gray
CRGB color6 = CRGB(255, 128, 0);    // Orange
```

### When to Use RGB

RGB is ideal when:
- You know the exact color you want
- You're working with web colors or hex values
- You need precise control over each channel
- You're blending specific colors

## HSV Color Model

HSV (Hue, Saturation, Value) represents colors differently:

- **Hue** (0-255): The color itself (position on color wheel)
- **Saturation** (0-255): Color intensity (0 = gray, 255 = vivid)
- **Value** (0-255): Brightness (0 = black, 255 = bright)

### Creating HSV Colors

```cpp
CHSV hsv1 = CHSV(0, 255, 255);      // Red
CHSV hsv2 = CHSV(96, 255, 255);     // Green
CHSV hsv3 = CHSV(160, 255, 255);    // Blue
CHSV hsv4 = CHSV(64, 255, 255);     // Yellow
CHSV hsv5 = CHSV(128, 255, 255);    // Cyan

// Automatic conversion to RGB
leds[0] = hsv1;
```

### Hue Values (0-255)

```
0   = Red
32  = Orange
64  = Yellow
96  = Green
128 = Cyan
160 = Blue
192 = Purple
224 = Pink
255 = Red (wraps around)
```

### When to Use HSV

HSV is ideal for:
- **Rainbow effects** - Just increment hue
- **Smooth color transitions** - Change hue gradually
- **Brightness control** - Adjust value without changing color
- **Color cycling** - Hue naturally wraps around

## HSV vs RGB Comparison

### Creating a Rainbow

**With RGB** (complex):
```cpp
// Must calculate each RGB component
for (int i = 0; i < NUM_LEDS; i++) {
    if (i % 6 == 0) leds[i] = CRGB(255, 0, 0);    // Red
    else if (i % 6 == 1) leds[i] = CRGB(255, 128, 0); // Orange
    else if (i % 6 == 2) leds[i] = CRGB(255, 255, 0); // Yellow
    // ... more cases
}
```

**With HSV** (simple):
```cpp
// Just increment hue
for (int i = 0; i < NUM_LEDS; i++) {
    uint8_t hue = (i * 255) / NUM_LEDS;
    leds[i] = CHSV(hue, 255, 255);
}
```

### Adjusting Brightness

**With RGB**:
```cpp
CRGB color = CRGB(200, 100, 50);
color.r = color.r * 0.5;  // Individually scale each
color.g = color.g * 0.5;
color.b = color.b * 0.5;
```

**With HSV**:
```cpp
CHSV color = CHSV(160, 255, 255);
color.v = 128;  // Simply change value
leds[0] = color;
```

## Saturation Effects

Saturation controls color intensity:

```cpp
uint8_t hue = 0;  // Red

leds[0] = CHSV(hue, 255, 255);  // Vivid red
leds[1] = CHSV(hue, 192, 255);  // Slightly desaturated
leds[2] = CHSV(hue, 128, 255);  // Pinkish
leds[3] = CHSV(hue, 64, 255);   // Very pale red
leds[4] = CHSV(hue, 0, 255);    // White (no saturation)
```

## Converting Between RGB and HSV

### HSV to RGB (Automatic)

```cpp
CHSV hsv = CHSV(128, 255, 255);
leds[0] = hsv;  // Automatic conversion
```

### RGB to HSV (Manual)

FastLED doesn't provide RGB-to-HSV conversion. If needed, use HSV from the start or implement your own converter.

## Practical Examples

### Smooth Color Fade

```cpp
void colorFade() {
    static uint8_t hue = 0;

    // Fill all LEDs with current hue
    fill_solid(leds, NUM_LEDS, CHSV(hue, 255, 255));

    hue++;  // Increment hue for smooth transition
}
```

### Temperature to Color

```cpp
CRGB temperatureColor(float temp) {
    // Map temperature (0-100°C) to hue (blue to red)
    uint8_t hue = map(temp, 0, 100, 160, 0);  // 160=blue, 0=red
    return CHSV(hue, 255, 255);
}
```

### Desaturate Effect

```cpp
void fadeToWhite() {
    static uint8_t saturation = 255;

    fill_solid(leds, NUM_LEDS, CHSV(96, saturation, 255));  // Green

    saturation--;  // Gradually desaturate to white
    if (saturation == 0) saturation = 255;  // Reset
}
```

## Color Mixing

### Additive (RGB)

```cpp
CRGB color1 = CRGB(100, 0, 0);   // Dim red
CRGB color2 = CRGB(0, 100, 0);   // Dim green
CRGB result = color1 + color2;   // Yellow (200, 100, 0)
```

### Blending

```cpp
CRGB color1 = CRGB::Red;
CRGB color2 = CRGB::Blue;

// Blend 50/50
CRGB result = blend(color1, color2, 128);  // Purple

// Blend 75% color2, 25% color1
CRGB result = blend(color1, color2, 192);
```

## Named Colors

FastLED provides many named colors:

```cpp
CRGB::Red
CRGB::Green
CRGB::Blue
CRGB::White
CRGB::Black
CRGB::Yellow
CRGB::Cyan
CRGB::Magenta
CRGB::Orange
CRGB::Purple
CRGB::Pink
// ... and many more
```

## Best Practices

### Use HSV for Animation

```cpp
// GOOD: Easy to animate
void rainbow() {
    static uint8_t hue = 0;
    for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = CHSV(hue + (i * 10), 255, 255);
    }
    hue++;
}
```

### Use RGB for Specific Colors

```cpp
// GOOD: Direct control
leds[0] = CRGB(255, 64, 32);  // Specific amber color
```

### Don't Mix Models Unnecessarily

```cpp
// AVOID: Converting back and forth
CHSV hsv = CHSV(128, 255, 255);
CRGB rgb = hsv;  // Convert to RGB
CHSV hsv2 = rgb;  // Can't convert back easily!

// BETTER: Stay in one model when possible
```

## Next Steps

- [Timing and Frame Rate](timing.md) - Control animation speed
- [Intermediate Techniques](../intermediate/) - Advanced color manipulation
