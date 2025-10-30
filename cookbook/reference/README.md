# FastLED Reference Guide

**Difficulty Level**: ⭐ All Levels
**Time to Find Information**: 2-5 minutes (depending on what you're looking up)
**Prerequisites**: None (reference materials are self-contained and can be used at any point in your learning journey)

**You'll Find**:
- Complete API function reference with syntax and parameters for all FastLED functions
- Platform-specific pin recommendations and limitations for Arduino, ESP32, Teensy, STM32, and more
- Supported LED chipset list with configuration examples for WS2812B, APA102, SK6812, and others
- Quick code snippets for common operations (setup, colors, array operations)
- External resources and community links for additional FastLED help

This section provides quick reference materials for FastLED development.

## Contents

- [API Quick Reference](api.md) - Comprehensive API function reference
- [Pin Configurations](pins.md) - Platform-specific pin recommendations
- [LED Types](led-types.md) - Supported LED chipsets and configuration

## Quick Navigation

### Common Operations

**Setup and Display**
```cpp
FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);
FastLED.show();
FastLED.setBrightness(50);
```

**Color Creation**
```cpp
CRGB color = CRGB(255, 0, 0);    // RGB
CHSV color = CHSV(0, 255, 255);  // HSV
```

**Array Operations**
```cpp
fill_solid(leds, NUM_LEDS, CRGB::Red);
fadeToBlackBy(leds, NUM_LEDS, 20);
blur1d(leds, NUM_LEDS, 50);
```

## Platform Support

FastLED supports a wide range of microcontrollers:
- Arduino (Uno, Mega, Leonardo, etc.)
- ESP32 / ESP8266
- Teensy (3.x, 4.x)
- STM32
- Raspberry Pi Pico (RP2040)
- And many more!

See [Pin Configurations](pins.md) for platform-specific details.

## LED Chipset Support

FastLED supports numerous LED chipsets including:
- WS2812B (NeoPixel)
- APA102 (DotStar)
- SK6812
- WS2813
- And many more!

See [LED Types](led-types.md) for complete list and usage examples.

## Additional Resources

- **FastLED Documentation**: https://fastled.io
- **GitHub Repository**: https://github.com/FastLED/FastLED
- **Community Wiki**: https://github.com/FastLED/FastLED/wiki
- **Main Cookbook**: [../COOKBOOK.md](../COOKBOOK.md)

## Navigation

[← Back to Cookbook](../COOKBOOK.md)
