# Hardware Requirements

Components needed for your FastLED project.

**Difficulty Level**: ⭐ Beginner
**Time to Complete**: 20 minutes (reading and shopping list preparation)
**Prerequisites**: None - start here if you're buying hardware
**You'll Learn**:
- Which microcontrollers work with FastLED and which to choose
- What LED strip types are compatible and their differences
- How to calculate power supply requirements for your project
- Essential safety components (capacitor, resistor) and why they matter
- How to properly wire everything together safely

## Essential Components

### Microcontroller

Any of these popular boards work with FastLED:

- **Arduino** (Uno, Nano, Mega, etc.)
- **ESP32** (recommended for WiFi projects)
- **ESP8266** (NodeMCU, Wemos D1 Mini)
- **Teensy** (3.x, 4.x)
- **STM32** (BluePill, etc.)
- **Raspberry Pi Pico** (RP2040)

### LED Strip

Popular LED strip types:

- **WS2812B** (most common, also called "NeoPixel")
- **APA102** (also called "DotStar", requires clock pin)
- **SK6812** (similar to WS2812B, RGBW available)
- **WS2813** (has backup data line)

### Power Supply

**Critical**: Your power supply must provide sufficient current.

- **Rule of thumb**: 60mA per LED at full white brightness
- **Example**: 100 LEDs = 6A minimum

| LED Count | Max Current | Recommended Supply |
|-----------|-------------|-------------------|
| 30        | 1.8A       | 2.5A              |
| 60        | 3.6A       | 5A                |
| 100       | 6.0A       | 8A                |
| 150       | 9.0A       | 12A               |

**Note**: Most patterns use 30-50% of max current. Use `FastLED.setMaxPowerInVoltsAndMilliamps()` to limit draw.

### Additional Components

**Highly Recommended**:
- **1000µF capacitor** - Place across power supply terminals to smooth voltage
- **220-470Ω resistor** - Place on data line near microcontroller

**Optional**:
- Level shifter (for 3.3V microcontrollers driving 5V LEDs)
- Breadboard and jumper wires (for prototyping)

## Wiring Diagram

Basic connection diagram:

```
Power Supply (+5V) ----+---> LED Strip VCC
                       |
                       +---> Microcontroller VIN/5V (if needed)

Power Supply (GND) ----+---> LED Strip GND
                       |
                       +---> Microcontroller GND (MUST share ground!)

Microcontroller GPIO --[220Ω resistor]---> LED Strip DATA

Power Supply (+) --[1000µF capacitor]-- Power Supply (-)
```

## Important Warnings

**Never**:
- Power long LED strips from the microcontroller's 5V pin (max 500mA typically)
- Run LEDs without a shared ground between power supply and microcontroller
- Connect/disconnect LEDs while powered on

**Always**:
- Use a dedicated power supply for LED strips with 30+ LEDs
- Connect GND from power supply to both LED strip AND microcontroller
- Add a capacitor across power supply terminals
- Start with low brightness when testing

## Next Steps

- [Installation and Setup](installation.md) - Set up your development environment
- [Your First Blink Example](first-example.md) - Test your hardware
