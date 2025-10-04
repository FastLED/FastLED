# ESP8266 UART Driver Example

This example demonstrates the **opt-in UART-based WS2812 driver** for ESP8266, which provides improved stability under Wi-Fi load compared to the default bit-bang driver.

## Overview

The ESP8266's single-core architecture and frequent NMI/RTOS interrupts can disrupt bit-bang timing loops, especially when Wi-Fi is active. This UART driver solves this problem by using the hardware UART1 peripheral to generate precise LED timing automatically.

## How It Works

The driver encodes WS2812 LED data into a UART byte stream at 3.2 Mbps:
- Each LED bit is represented by 4 UART bits (1.25 µs total)
- "0" bit → `1000` (0.35 µs high, 0.8 µs low)
- "1" bit → `1100` (0.7 µs high, 0.6 µs low)
- Two LED bits are packed into each UART byte using a lookup table

## Usage

### Enable the UART driver

**Option 1: Define before including FastLED**
```cpp
#define FASTLED_ESP8266_UART
#include <FastLED.h>

CRGB leds[NUM_LEDS];

void setup() {
    FastLED.addLeds<UARTController_ESP8266<GRB>>(leds, NUM_LEDS);
}
```

**Option 2: Explicit controller type**
```cpp
#include <FastLED.h>

CRGB leds[NUM_LEDS];

void setup() {
    FastLED.addLeds<UARTController_ESP8266<GRB>>(leds, NUM_LEDS);
}
```

## Advantages

✅ **Hardware timing** - UART peripheral shifts bits automatically
✅ **Minimal CPU overhead** - Main loop remains responsive
✅ **Wi-Fi stable** - No NMI interrupt timing issues
✅ **Precise timing** - Maintains ±150ns WS2812 tolerance

## Trade-offs

⚠️ **Higher RAM usage** - ~12 bytes per LED (300 LEDs = 3.6 KB buffer)
⚠️ **Single pin only** - GPIO2 (UART1 TX-only on ESP8266)
⚠️ **No parallel output** - Not compatible with multi-strip modes

## Pin Assignment

- **GPIO2** - UART1 TX (the only usable pin for this driver)
- This is the same as D4 on NodeMCU boards

## Configuration

You can optionally configure the baud rate and reset time:

```cpp
UARTController_ESP8266<GRB> controller;
controller.setBaud(3200000);      // Default: 3.2 Mbps
controller.setResetTimeUs(300);   // Default: 300 µs (WS2812 requires >= 50 µs)
FastLED.addLeds(&controller, leds, NUM_LEDS);
```

## Performance

- **Frame rate**: Up to 30-60 FPS for typical strip sizes (<1000 LEDs)
- **Latency**: ~11.25 ms for 300 LEDs (3600 bytes @ 3.2 Mbps + 300 µs latch)
- **Memory overhead**: 12 bytes per LED (temporary encode buffer)

## When to Use This Driver

Use the UART driver when:
- You're experiencing LED flickering with Wi-Fi enabled
- Your application needs reliable Wi-Fi + LED operation
- You have RAM available for the encode buffer
- You only need a single LED strip

Stick with the default bit-bang driver when:
- You need multiple parallel LED strips
- RAM is constrained
- Wi-Fi is not in use
- You need pins other than GPIO2

## Credits

This implementation is based on the proven technique from the NeoPixelBus library, adapted to work seamlessly with FastLED's API and features.
