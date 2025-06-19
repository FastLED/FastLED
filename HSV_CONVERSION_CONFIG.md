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

## Detailed Comparison

From `tests/test_hsv_conversion_accuracy.cpp`

```
=== HSV to RGB Conversion Accuracy Test ===
Testing RGB -> HSV -> RGB round-trip accuracy
Sampling every 8th RGB value for comprehensive coverage


hsv2rgb_rainbow Error Statistics:
  Average: 82.197899
  Median:  64.660652
  Min:     0.000000
  Max:     301.333710
  Samples: 32768

hsv2rgb_spectrum Error Statistics:
  Average: 96.715103
  Median:  93.220169
  Min:     0.000000
  Max:     278.786652
  Samples: 32768

hsv2rgb_fullspectrum Error Statistics:
  Average: 119.301537
  Median:  119.607689
  Min:     0.000000
  Max:     364.731415
  Samples: 32768

=== Error Comparison ===
Function            Average    Median     Min        Max
hsv2rgb_rainbow     82.197899   64.660652   0.000000   301.333710
hsv2rgb_spectrum    96.715103   93.220169   0.000000   278.786652
hsv2rgb_fullspectrum119.301537   119.607689   0.000000   364.731415

=== Best Performance Rankings ===
Lowest Average Error: rainbow (82.197899)
Lowest Median Error:  rainbow (64.660652)
Lowest Max Error:     spectrum (278.786652)


=== Specific Color Conversion Tests ===
Color           Original RGB    Rainbow RGB     Spectrum RGB    FullSpectrum RGB
-------------   -----------     -----------     ------------    ----------------
Pure Red        (255,  0,  0)   (253,  2,  0)   (251,  0,  0)   (255,  6,  0)
Pure Green      (  0,255,  0)   (  0,255,  0)   (  0,219, 31)   (  0,255, 64)
Pure Blue       (  0,  0,255)   (  2,  0,253)   (  0, 27,223)   (  0, 58,255)
Yellow          (255,255,  0)   (171,170,  0)   ( 59,191,  0)   (128,255,  0)
Magenta         (255,  0,255)   (253,  2,  0)   (251,  0,  0)   (255,  6,  0)
Cyan            (  0,255,255)   (  2,  0,253)   (  0, 27,223)   (  0, 58,255)
White           (255,255,255)   (255,255,255)   (254,254,254)   (255,255,255)
Black           (  0,  0,  0)   (  0,  0,  0)   (  0,  0,  0)   (  0,  0,  0)
Gray            (128,128,128)   (128,128,128)   (180,180,180)   (181,181,181)
Orange          (255,128,  0)   (  0,212, 43)   (  0,171, 79)   (  0,255,160)
Purple          (128,  0,255)   (130,  0,126)   (111,  0,139)   (230,  0,255)
Pink            (255,192,203)   (247,191,199)   (249,220,224)   (255,221,232)


=== Hue Sweep Conversion Test ===
Testing full hue range at maximum saturation and brightness
Hue   Rainbow RGB     Spectrum RGB    FullSpectrum RGB
----  -----------     ------------    ----------------
  0   (255,  0,  0)   (251,  0,  0)   (255,  0,  0)
 32   (171, 85,  0)   (155, 95,  0)   (255,192,  0)
 64   (171,170,  0)   ( 59,191,  0)   (128,255,  0)
 96   (  0,255,  0)   (  0,219, 31)   (  0,255, 64)
128   (  0,171, 85)   (  0,123,127)   (  0,255,255)
160   (  0,  0,255)   (  0, 27,223)   (  0, 64,255)
192   ( 85,  0,171)   ( 63,  0,187)   (128,  0,255)
224   (170,  0, 85)   (159,  0, 91)   (255,  0,192)
```