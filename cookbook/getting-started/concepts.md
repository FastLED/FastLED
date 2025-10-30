# Basic Concepts

**Difficulty Level**: ⭐ Beginner
**Time to Complete**: 20 minutes
**Prerequisites**: None (this is your starting point!)

**You'll Learn**:
- How to address and control individual LEDs in a strip
- The difference between RGB and HSV color models and when to use each
- The essential setup() and loop() pattern for FastLED programs
- How to configure FastLED for your specific LED hardware
- Key concepts like brightness control and static variables

Core ideas behind LED programming with FastLED.

## LED Addressing

Each LED in your strip has an **index** (position) starting from 0.

```
LED Strip:  [0] [1] [2] [3] [4] [5] ... [59]
             ^                           ^
           First                       Last
```

You control LEDs by setting color values in an array:

```cpp
CRGB leds[60];  // Array for 60 LEDs

leds[0] = CRGB::Red;    // First LED is red
leds[5] = CRGB::Blue;   // Sixth LED is blue
```

## Color Models

FastLED supports two main color models:

### RGB (Red, Green, Blue)

Each color channel ranges from 0-255:

```cpp
CRGB color = CRGB(255, 0, 0);      // Red
CRGB color = CRGB(0, 255, 0);      // Green
CRGB color = CRGB(0, 0, 255);      // Blue
CRGB color = CRGB(255, 255, 255);  // White
```

### HSV (Hue, Saturation, Value)

- **Hue** (0-255): Color (red → orange → yellow → green → blue → purple → red)
- **Saturation** (0-255): Color intensity (0 = white, 255 = full color)
- **Value** (0-255): Brightness (0 = off, 255 = full brightness)

```cpp
CHSV hsv1 = CHSV(0, 255, 255);      // Red
CHSV hsv2 = CHSV(96, 255, 255);     // Green
CHSV hsv3 = CHSV(160, 255, 255);    // Blue

// Automatic conversion to RGB
leds[0] = hsv1;
```

**Why use HSV?**
- Easier to create smooth color transitions (just change hue)
- Natural for rainbow effects
- Simple brightness control without changing color

## The Main Loop Pattern

Every FastLED program follows this pattern:

```cpp
void setup() {
    // 1. Initialize FastLED (runs once)
    FastLED.addLeds<LED_TYPE, DATA_PIN>(leds, NUM_LEDS);
    FastLED.setBrightness(50);
}

void loop() {
    // 2. Calculate/update LED colors
    leds[0] = CRGB::Red;

    // 3. Send colors to LEDs
    FastLED.show();

    // 4. Optional: delay or manage frame rate
    delay(20);
}
```

**Key points**:
- `setup()` runs once at startup - configure your LEDs here
- `loop()` runs repeatedly - update your LED colors here
- **You must call `FastLED.show()`** to actually update the physical LEDs
- Changes to the `leds[]` array are not visible until you call `FastLED.show()`

## LED Type Configuration

Tell FastLED about your hardware:

```cpp
#define LED_PIN     5           // Data pin number
#define NUM_LEDS    60          // Number of LEDs
#define LED_TYPE    WS2812B     // Your LED strip type
#define COLOR_ORDER GRB         // Color channel order

CRGB leds[NUM_LEDS];

void setup() {
    FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
}
```

**Common LED types**:
- `WS2812B` - Most common (NeoPixel)
- `APA102` - DotStar (requires clock pin)
- `SK6812` - Similar to WS2812B

**Common color orders**:
- `GRB` - WS2812B (most common)
- `RGB` - Some WS2811
- `BGR` - APA102

If your colors are wrong, try different `COLOR_ORDER` values.

## Brightness Control

Control overall brightness (0-255):

```cpp
void setup() {
    FastLED.addLeds<LED_TYPE, LED_PIN>(leds, NUM_LEDS);
    FastLED.setBrightness(50);  // 0-255 (50 = ~20% brightness)
}
```

**Tip**: Start with low brightness (50) when testing to avoid eye strain and reduce power draw.

## Static vs Dynamic Variables

Use `static` variables to remember values between loop iterations:

```cpp
void loop() {
    static uint8_t hue = 0;  // Remembers value across loops

    leds[0] = CHSV(hue, 255, 255);
    FastLED.show();

    hue++;  // Increment for next loop
}
```

Without `static`, the variable resets to 0 every loop.

## Next Steps

Now that you understand the basics:

- [Your First Blink Example](first-example.md) - Write your first program
- [Core Concepts](../core-concepts/) - Deeper dive into LED programming
