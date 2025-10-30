# Pin Configurations by Platform

**Difficulty Level**: ⭐ All Levels
**Time to Find Information**: 3-5 minutes
**Prerequisites**: None (quick reference guide)

**You'll Find**:
- Recommended data pins for Arduino Uno, Mega, ESP32, ESP8266, Teensy, STM32, and RP2040
- Pin limitations and restrictions per platform (boot pins, input-only pins, flash pins)
- Best pins for hardware-accelerated output (ESP32 RMT, RP2040 PIO)
- Memory capacity and maximum LED counts for each microcontroller
- Multiple strip wiring examples with pin assignments
- Level shifting guidance for 3.3V to 5V signal conversion
- Troubleshooting solutions for first LED issues, signal degradation, and WiFi interference

---

Platform-specific pin recommendations and configurations for FastLED.

## Arduino Uno/Nano (ATmega328)

```cpp
// Any digital pin can be used for data
#define DATA_PIN 6

// Recommended pins for best performance:
// Pins 2-13 (all digital pins work)
// Avoid pins 0,1 (Serial), 13 (LED)
```

## Arduino Mega (ATmega2560)

```cpp
#define DATA_PIN 6

// Many pins available: 2-53
// Best performance: pins 2-13, 44-46
```

## ESP32

```cpp
// Most GPIO pins work, but avoid:
// GPIO 0, 2 (boot pins)
// GPIO 6-11 (flash pins)
// GPIO 34-39 (input only)

// Recommended pins:
#define DATA_PIN 16  // or 17, 18, 19, 21, 22, 23

// For RMT-based output (best):
// GPIO 2, 4, 5, 12-19, 21-23, 25-27, 32-33

// Multiple strips example:
#define STRIP1_PIN 16
#define STRIP2_PIN 17
#define STRIP3_PIN 18
```

## ESP8266

```cpp
// Limited pins available
#define DATA_PIN 2   // GPIO2 (D4 on NodeMCU)

// Other options:
// GPIO0 (D3), GPIO2 (D4), GPIO4 (D2), GPIO5 (D1)
// GPIO12 (D6), GPIO13 (D7), GPIO14 (D5), GPIO15 (D8)

// Avoid during boot: GPIO0, GPIO2, GPIO15
```

## Teensy 4.x

```cpp
// Most pins work, excellent performance
#define DATA_PIN 7

// Recommended for WS2812B: Any pin
// Clock pins for APA102: 13, 14
```

## STM32 (BluePill)

```cpp
#define DATA_PIN PA7

// Available pins: Most GPIO pins
// SPI for APA102: PA5 (SCK), PA7 (MOSI)
```

## Raspberry Pi Pico (RP2040)

```cpp
#define DATA_PIN 16

// GPIO pins 0-29 available
// PIO-based output provides excellent performance
```

## Platform Comparison

| Platform | Max LEDs | Notes |
|----------|----------|-------|
| Arduino Uno | ~500 | Limited by RAM (2KB) |
| Arduino Mega | ~2000 | More RAM (8KB) |
| ESP32 | 1000+ | Use PSRAM for more |
| ESP8266 | ~500 | Limited RAM |
| Teensy 4.0 | 5000+ | Very fast, lots of RAM |
| STM32 | 1000+ | Good performance |
| RP2040 | 1000+ | Excellent with PIO |

## Memory Calculation

Each LED uses 3 bytes (RGB):

```
Example: 500 LEDs = 1500 bytes

Arduino Uno: 2KB RAM total
- System: ~500 bytes
- Your code: ~500 bytes
- LEDs: ~1000 bytes (333 LEDs max)
```

## Best Practices

### Pin Selection

1. **Avoid boot/strapping pins** - These can interfere with programming
2. **Avoid input-only pins** - Some platforms have input-only GPIO
3. **Check voltage levels** - Ensure 3.3V outputs are compatible with 5V LEDs
4. **Use hardware peripherals** - RMT (ESP32), PIO (RP2040) for best performance

### Multiple Strips

```cpp
// Define multiple strips on different pins
CRGB strip1[LEDS_PER_STRIP];
CRGB strip2[LEDS_PER_STRIP];
CRGB strip3[LEDS_PER_STRIP];

void setup() {
    FastLED.addLeds<WS2812B, 5, GRB>(strip1, LEDS_PER_STRIP);
    FastLED.addLeds<WS2812B, 6, GRB>(strip2, LEDS_PER_STRIP);
    FastLED.addLeds<WS2812B, 7, GRB>(strip3, LEDS_PER_STRIP);
}
```

### Level Shifting

For 3.3V microcontrollers driving 5V LEDs:
- **Option 1**: Use a level shifter (74HCT245 or similar)
- **Option 2**: Try direct connection (often works but not guaranteed)
- **Option 3**: Use RGBW LEDs (often more tolerant of 3.3V signals)

## Troubleshooting

### First LED Wrong/Flickering

**Problem**: First LED shows wrong color or flickers
**Solution**: Add 220-470Ω resistor on data line (close to microcontroller)

### Signal Issues on Long Runs

**Problem**: LEDs fail after certain distance
**Solution**:
- Keep data wire short (< 6 inches ideally)
- Use level shifter for longer runs
- Consider using APA102 (clock + data) for better signal integrity

### ESP32 WiFi Interference

**Problem**: LEDs glitch during WiFi activity
**Solution**: Use RMT pins (GPIO 2, 4, 5, 12-19, 21-23, 25-27, 32-33)

## Navigation

- [← Back to Reference](README.md)
- [API Quick Reference](api.md)
- [LED Types](led-types.md)
- [← Back to Cookbook](../COOKBOOK.md)
