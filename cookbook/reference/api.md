# API Quick Reference

**Difficulty Level**: ⭐ All Levels
**Time to Find Information**: 2-5 minutes
**Prerequisites**: None (quick lookup reference)

**You'll Find**:
- Complete function syntax with parameters for all major FastLED APIs
- Setup functions for adding LEDs, setting brightness, and power management
- Display and timing functions for updating and controlling animations
- Color creation and manipulation functions for both RGB and HSV
- Array operations for filling, fading, and blurring LED strips
- Mathematical functions for scaling, mapping, waves, and beats
- Noise and random functions for organic patterns
- Color palette APIs with built-in palette list
- Quick copy-paste syntax for common operations

---

Comprehensive reference for FastLED functions and APIs.

## Setup Functions

### Adding LEDs

```cpp
// Add LEDs (single pin)
FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);

// Add LEDs with clock pin (APA102, etc.)
FastLED.addLeds<LED_TYPE, DATA_PIN, CLOCK_PIN, COLOR_ORDER>(leds, NUM_LEDS);
```

### Brightness and Power

```cpp
// Set global brightness (0-255)
FastLED.setBrightness(value);

// Set maximum power draw
FastLED.setMaxPowerInVoltsAndMilliamps(volts, milliamps);
```

### Visual Settings

```cpp
// Set dithering mode
FastLED.setDither(BINARY_DITHER);  // or DISABLE_DITHER

// Set color correction
FastLED.setCorrection(TypicalLEDStrip);  // or TypicalPixelString

// Set color temperature
FastLED.setTemperature(Candle);  // or others
```

## Display Functions

```cpp
FastLED.show();              // Update LEDs
FastLED.clear();             // Set all to black
FastLED.clear(true);         // Clear and show
FastLED.delay(ms);           // Delay and handle timing
uint16_t fps = FastLED.getFPS();  // Get current frame rate
```

## Color Functions

### Creating Colors

```cpp
// Create colors
CRGB color = CRGB(r, g, b);         // RGB
CHSV color = CHSV(h, s, v);         // HSV
CRGB color = CRGB::Red;             // Named colors
```

### Color Manipulation

```cpp
// Blend colors
CRGB result = blend(color1, color2, amount);

// Fade colors
color.fadeToBlackBy(amount);        // 0-255
color.nscale8(scale);               // Scale brightness

// Add colors
leds[i] += CRGB(10, 0, 0);         // Add red
```

## Array Fill Functions

```cpp
fill_solid(leds, NUM_LEDS, color);
fill_rainbow(leds, NUM_LEDS, startHue, deltaHue);
fill_gradient_RGB(leds, NUM_LEDS, color1, color2);
fill_gradient_RGB(leds, NUM_LEDS, color1, color2, color3);
```

## Array Fade Functions

```cpp
fadeToBlackBy(leds, NUM_LEDS, amount);     // Fade toward black
fadeLightBy(leds, NUM_LEDS, amount);       // Same as fadeToBlackBy
nscale8(leds, NUM_LEDS, scale);            // Scale brightness
```

## Array Blur Functions

```cpp
blur1d(leds, NUM_LEDS, amount);            // Blur 1D array
```

## Math Functions

### Scaling

```cpp
uint8_t scale8(uint8_t i, uint8_t scale);              // i * scale / 255
uint16_t scale16(uint16_t i, uint16_t scale);          // 16-bit version
```

### Mapping

```cpp
uint8_t map(value, fromLow, fromHigh, toLow, toHigh);
```

### Adding/Subtracting with Saturation

```cpp
uint8_t qadd8(uint8_t i, uint8_t j);                   // Add, max 255
uint8_t qsub8(uint8_t i, uint8_t j);                   // Subtract, min 0
```

### Sine Waves

```cpp
uint8_t sin8(uint8_t theta);                           // Fast 8-bit sine
uint16_t sin16(uint16_t theta);                        // 16-bit sine
```

### Beat Functions

```cpp
uint8_t beat8(uint16_t bpm);                           // Sawtooth
uint8_t beatsin8(uint16_t bpm, uint8_t min, uint8_t max);  // Sine wave
```

## Random Functions

```cpp
uint8_t random8();                     // 0-255
uint8_t random8(uint8_t max);         // 0 to max-1
uint8_t random8(uint8_t min, uint8_t max);
uint16_t random16();                   // 0-65535
uint16_t random16(uint16_t max);
```

## Noise Functions

```cpp
uint8_t inoise8(uint16_t x);
uint8_t inoise8(uint16_t x, uint16_t y);
uint8_t inoise8(uint16_t x, uint16_t y, uint16_t z);
uint16_t inoise16(uint32_t x, uint32_t y, uint32_t z);
```

## Timing Macros

```cpp
EVERY_N_MILLISECONDS(ms) { /* code */ }
EVERY_N_SECONDS(seconds) { /* code */ }

// Get current time
uint32_t now = millis();
```

## Color Palettes

### Built-in Palettes

```cpp
RainbowColors_p
RainbowStripeColors_p
CloudColors_p
PartyColors_p
OceanColors_p
LavaColors_p
ForestColors_p
HeatColors_p
```

### Using Palettes

```cpp
// Use palette
CRGB color = ColorFromPalette(palette, index, brightness, blendType);
// blendType: LINEARBLEND or NOBLEND
```

## Navigation

- [← Back to Reference](README.md)
- [Pin Configurations](pins.md)
- [LED Types](led-types.md)
- [← Back to Cookbook](../COOKBOOK.md)
