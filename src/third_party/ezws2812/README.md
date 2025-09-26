# ezWS2812 Driver for Silicon Labs MGM240/MG24

This directory contains the ezWS2812 driver from Silicon Labs, adapted for use with FastLED.

## Origin

Based on the Silicon Labs ezWS2812 library version 2.2.0 by Tamas Jozsi.
- Original repository: https://github.com/SiliconLabs/arduino
- Path: libraries/ezWS2812

## Purpose

Provides optimized WS2812 LED control for Silicon Labs EFR32MG24 series microcontrollers, including:
- Arduino Nano Matter (MGM240)
- SparkFun Thing Plus Matter (MGM240)
- Seeed Xiao MG24 Sense
- Other EFR32MG24-based boards

## Available Controllers

### 1. EZWS2812_GPIO (Always Available)
- Uses optimized GPIO bit-banging with precise timing
- Calibrated for 39MHz and 78MHz CPU frequencies
- Does not consume any hardware peripherals
- Good performance with minimal resource usage

### 2. EZWS2812_SPI (Optional)
- Uses hardware SPI peripheral for WS2812 signal generation
- Higher performance and allows CPU to do other tasks
- **Consumes one SPI peripheral**
- Must be explicitly enabled with `#define FASTLED_USES_EZWS2812_SPI`

## Usage

### GPIO Controller (Default)
```cpp
#include <FastLED.h>

CRGB leds[NUM_LEDS];

void setup() {
    // GPIO-based controller on pin 7
    FastLED.addLeds<EZWS2812_GPIO, 7, GRB>(leds, NUM_LEDS);
}
```

### SPI Controller (Optional)
```cpp
// IMPORTANT: Define this BEFORE including FastLED.h
#define FASTLED_USES_EZWS2812_SPI
#include <FastLED.h>

CRGB leds[NUM_LEDS];

void setup() {
    // SPI-based controller (uses hardware SPI)
    FastLED.addLeds<EZWS2812_SPI, GRB>(leds, NUM_LEDS);
}
```

## Why SPI Requires Opt-In

The SPI controller consumes a hardware SPI peripheral, which may be needed for other purposes in your project (SD cards, displays, sensors, etc.). Following the same pattern as ObjectFLED for Teensy, the SPI controller must be explicitly enabled to prevent accidentally consuming the SPI peripheral.

## Define Summary

- **`FASTLED_USES_EZWS2812_SPI`** - Enables SPI-based WS2812 controller for MG24 chips
  - Must be defined **before** `#include <FastLED.h>`
  - Enables `EZWS2812_SPI` template alias
  - Consumes hardware SPI peripheral

## Technical Details

### GPIO Controller
- Timing generated using calibrated NOP delays
- Interrupts disabled during transmission
- Supports both 39MHz and 78MHz CPU frequencies
- ~400Hz maximum refresh rate

### SPI Controller
- Each WS2812 bit encoded as 8 SPI bits
- SPI clock: 3.2MHz for proper WS2812 timing
- Pattern: '1' bit = 0xFC, '0' bit = 0x80
- ~1000Hz maximum refresh rate

## License

MIT License (see ezWS2812.h for full text)
Copyright (c) 2024 Silicon Laboratories Inc.