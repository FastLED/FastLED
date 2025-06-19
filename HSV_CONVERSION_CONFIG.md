# Configurable HSV Conversion in FastLED

FastLED now supports configurable HSV to RGB conversion algorithms. This allows you to choose between different conversion methods depending on your specific needs and preferences.

## Available Conversion Methods

### 1. Rainbow (Default)
This is the traditional FastLED HSV conversion that produces a "visually balanced rainbow" with enhanced yellow and orange tones. This remains the default behavior for backward compatibility.

### 2. Spectrum
A mathematically straight spectrum conversion that produces more green and blue tones, with less yellow and orange compared to the rainbow method.

### 3. Full Spectrum
Uses the standard HSV to RGB conversion algorithm from Wikipedia, providing a full mathematical spectrum.

## How to Configure

To select a specific HSV conversion method, define one of the following preprocessor symbols **before** including FastLED.h:

```cpp
// For spectrum conversion
#define FASTLED_HSV_CONVERSION_SPECTRUM
#include <FastLED.h>

// For full spectrum conversion  
#define FASTLED_HSV_CONVERSION_FULL_SPECTRUM
#include <FastLED.h>

// For explicit rainbow conversion
#define FASTLED_HSV_CONVERSION_RAINBOW
#include <FastLED.h>

// Default behavior (no define needed) - uses rainbow
#include <FastLED.h>
```

## What Gets Affected

The configuration affects all HSV to RGB conversions in your code, including:

- `CRGB` constructor from `CHSV`: `CRGB rgb(hsv);`
- `CRGB` assignment from `CHSV`: `rgb = hsv;`
- `setHSV()` method: `rgb.setHSV(hue, sat, val);`
- `setHue()` method: `rgb.setHue(hue);`
- Array-based conversions when using `fill_rainbow()` and similar functions
- Any direct calls to `hsv2rgb_dispatch()` functions

## Example Usage

```cpp
#define FASTLED_HSV_CONVERSION_SPECTRUM  // Choose spectrum conversion
#include <FastLED.h>

#define NUM_LEDS 60
CRGB leds[NUM_LEDS];

void setup() {
    FastLED.addLeds<WS2812, 6, GRB>(leds, NUM_LEDS);
    
    // All of these will use spectrum conversion
    leds[0] = CHSV(128, 255, 255);  // Constructor conversion
    leds[1].setHSV(64, 255, 255);   // setHSV method
    leds[2].setHue(192);            // setHue method
    
    // Array-based conversion also uses the configured method
    fill_rainbow(leds, NUM_LEDS, 0, 4);
}
```

## Comparison of Methods

Here's how different hue values convert to RGB using each method:

| Hue | Rainbow | Spectrum | Full Spectrum |
|-----|---------|----------|---------------|
| 0   | Red tones with enhanced warmth | Pure mathematical red | Standard RGB red |
| 64  | Enhanced yellow/orange | Less saturated yellow | Mathematical yellow |
| 128 | Balanced cyan/aqua | More blue-green | Standard cyan |
| 192 | Balanced purple/magenta | More blue bias | Mathematical magenta |

## Backward Compatibility

If no define is specified, FastLED uses the traditional rainbow conversion method, ensuring that existing code continues to work exactly as before.

## Performance

All conversion methods have similar performance characteristics. The choice between them is primarily aesthetic and depends on your specific application requirements.

## Advanced Usage

For applications that need to use different conversion methods in different parts of the code, you can still call the specific conversion functions directly:

```cpp
#include <FastLED.h>

CHSV hsv(128, 255, 255);
CRGB rgb1, rgb2, rgb3;

// Use specific conversion methods regardless of global setting
hsv2rgb_rainbow(hsv, rgb1);
hsv2rgb_spectrum(hsv, rgb2);  
hsv2rgb_fullspectrum(hsv, rgb3);
```

## Testing

The `examples/HSVTest/HSVTest.ino` file provides a comprehensive test of the different conversion methods. Compile it with different defines to see the differences in output. 