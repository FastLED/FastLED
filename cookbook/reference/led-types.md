# Supported LED Types

Comprehensive guide to LED chipsets supported by FastLED.

**Difficulty Level**: ⭐ All Levels

**Time to Find Information**: 3-5 minutes

**Prerequisites**: None

**You'll Find**:
- Complete reference table of common LED chipsets (WS2812B, APA102, SK6812, WS2813, SK9822, WS2801, LPD8806, P9813) with voltage, speed, and key features
- Code examples for each chipset with correct LED_TYPE and COLOR_ORDER configuration for immediate copy-paste use
- Chipset selection guide comparing cost, speed, reliability, and use cases (when to use WS2812B vs APA102 vs SK6812 vs WS2813)
- Color order reference and testing code (GRB, RGB, BGR) with diagnostic test pattern to identify correct COLOR_ORDER for your specific LEDs
- Technical specifications including data rates, color depth, voltage requirements, current draw, and timing tolerance for major chipsets
- Wiring diagrams for single-wire LEDs (WS2812B family) and clock+data LEDs (APA102 family) with resistor placement
- Performance comparison table rating speed, reliability, cost, ease of use, and availability across popular LED types
- Troubleshooting guide for color order issues, first LED problems, partial failures, and random flickering

## Common LED Types

| LED Type | Clock Pin | Voltage | Speed | Notes |
|----------|-----------|---------|-------|-------|
| WS2812B | No | 5V | 800 KHz | Most popular, "NeoPixel" |
| SK6812 | No | 5V | 800 KHz | Similar to WS2812B, RGBW available |
| WS2813 | No | 5V | 800 KHz | Backup data line |
| APA102 | Yes | 5V | 1-20 MHz | "DotStar", faster refresh |
| SK9822 | Yes | 5V | 1-20 MHz | Similar to APA102 |
| WS2801 | Yes | 5V | 25 MHz | Older, requires clock |
| LPD8806 | Yes | 5V | 20 MHz | Older strip type |
| P9813 | Yes | 5V | 15 MHz | Common in modules |

## Code Examples by Type

### WS2812B / NeoPixel

```cpp
#define LED_TYPE WS2812B
#define COLOR_ORDER GRB
FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);
```

**Characteristics:**
- Single data wire (no clock)
- 800 KHz data rate
- Most common and widely available
- Good for most applications
- Budget-friendly

### APA102 / DotStar

```cpp
#define LED_TYPE APA102
#define COLOR_ORDER BGR
FastLED.addLeds<LED_TYPE, DATA_PIN, CLOCK_PIN, COLOR_ORDER>(leds, NUM_LEDS);
```

**Characteristics:**
- Separate clock and data lines
- Much faster refresh rates (up to 20 MHz)
- Better color accuracy
- Good for long cable runs
- More expensive than WS2812B

### SK6812 (RGBW)

```cpp
// Note: FastLED doesn't natively support RGBW, use workaround:
#define LED_TYPE SK6812
#define COLOR_ORDER GRB
FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);
```

**Characteristics:**
- Similar to WS2812B
- RGBW variant available (adds white channel)
- Good for white light applications
- FastLED treats as RGB only

### WS2813

```cpp
#define LED_TYPE WS2813
#define COLOR_ORDER GRB
FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);
```

**Characteristics:**
- Backup data line for reliability
- Compatible with WS2812B protocol
- More robust than WS2812B
- Single LED failure won't break chain

## Color Order Reference

Different LED types use different color orders:

```cpp
// Common orders:
GRB  // WS2812B, WS2813, most strips
RGB  // Some WS2811, APA106
BGR  // APA102, SK9822
BRG  // Rare
```

### Testing Color Order

```cpp
void testColorOrder() {
    leds[0] = CRGB(255, 0, 0);  // Should show RED
    leds[1] = CRGB(0, 255, 0);  // Should show GREEN
    leds[2] = CRGB(0, 0, 255);  // Should show BLUE
    FastLED.show();

    // If wrong, try different COLOR_ORDER in setup
}
```

## Choosing LED Type

### Use WS2812B when:

- Budget-friendly option needed
- Single data wire preferred
- Speed isn't critical (<400 Hz refresh)
- Most common and compatible

**Pros:**
- Inexpensive
- Widely available
- Single wire simplicity
- Good community support

**Cons:**
- Timing-sensitive
- Slower refresh rates
- Less reliable over long distances

### Use APA102 when:

- Need higher refresh rates (>400 Hz)
- Want better color accuracy
- Have SPI pins available
- Need longer cable runs

**Pros:**
- Very fast refresh
- Separate clock line (more reliable)
- Better signal integrity
- Not timing-critical

**Cons:**
- More expensive
- Requires 2 wires (data + clock)
- Uses more pins on microcontroller

### Use SK6812 when:

- Need RGBW (true white) capability
- Want WS2812B compatibility
- Working with white light displays

**Pros:**
- RGBW variants available
- Compatible with WS2812B code
- Good white light quality

**Cons:**
- Limited FastLED RGBW support
- Slightly more expensive than WS2812B

### Use WS2813 when:

- Need reliability in long installations
- Want backup data line feature
- Concerned about single LED failures

**Pros:**
- Backup data line (more robust)
- WS2812B compatible
- Chain continues if LED fails

**Cons:**
- More expensive than WS2812B
- Still timing-sensitive like WS2812B

## Technical Specifications

### WS2812B

```
Data Rate:        800 KHz
Color Depth:      8-bit per channel (24-bit RGB)
Voltage:          5V (±0.5V)
Current per LED:  ~60mA at full white
Protocol:         Single wire, timing-based
Timing Tolerance: ±150ns
```

### APA102

```
Data Rate:        Variable, up to 20 MHz
Color Depth:      8-bit per channel + 5-bit global brightness
Voltage:          5V (±0.5V)
Current per LED:  ~20mA at full white
Protocol:         SPI-like (clock + data)
Timing Tolerance: Not critical (clocked)
```

### SK6812

```
Data Rate:        800 KHz
Color Depth:      8-bit per channel (24-bit RGB or 32-bit RGBW)
Voltage:          5V (±0.5V)
Current per LED:  ~60mA at full white (RGB), ~80mA (RGBW)
Protocol:         Single wire, timing-based
Timing Tolerance: ±150ns
```

## Wiring Guidelines

### Single Wire LEDs (WS2812B, SK6812, WS2813)

```
Microcontroller GPIO ──[220Ω resistor]──> LED Strip DATA
Microcontroller GND  ───────────────────> LED Strip GND
Power Supply 5V      ───────────────────> LED Strip VCC
Power Supply GND     ───────────────────> LED Strip GND + MCU GND
```

### Clock + Data LEDs (APA102, SK9822)

```
Microcontroller MOSI ──[220Ω resistor]──> LED Strip DATA
Microcontroller SCK  ──[220Ω resistor]──> LED Strip CLOCK
Microcontroller GND  ───────────────────> LED Strip GND
Power Supply 5V      ───────────────────> LED Strip VCC
Power Supply GND     ───────────────────> LED Strip GND + MCU GND
```

## Common Issues

### Wrong Colors Displayed

**Problem**: LEDs show incorrect colors
**Solution**: Try different `COLOR_ORDER` values (GRB, RGB, BGR, etc.)

### First LED Flickers or Wrong

**Problem**: First LED unreliable
**Solution**: Add 220-470Ω resistor on data line near microcontroller

### LEDs Work Partially

**Problem**: First N LEDs work, rest don't
**Solution**:
- Check power supply capacity
- Look for damaged LED at cutoff point
- Add power injection every 60-100 LEDs

### Random Flickering

**Problem**: LEDs flicker randomly
**Solution**:
- Add 1000µF capacitor across power supply
- Ensure solid ground connections
- Check power supply capacity

## Performance Comparison

| Feature | WS2812B | APA102 | SK6812 | WS2813 |
|---------|---------|--------|--------|--------|
| Speed | ⭐⭐ | ⭐⭐⭐⭐⭐ | ⭐⭐ | ⭐⭐ |
| Reliability | ⭐⭐⭐ | ⭐⭐⭐⭐⭐ | ⭐⭐⭐ | ⭐⭐⭐⭐ |
| Cost | ⭐⭐⭐⭐⭐ | ⭐⭐⭐ | ⭐⭐⭐⭐ | ⭐⭐⭐⭐ |
| Ease of Use | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐ | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐⭐ |
| Availability | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐ | ⭐⭐⭐⭐ | ⭐⭐⭐ |

## Navigation

- [← Back to Reference](README.md)
- [API Quick Reference](api.md)
- [Pin Configurations](pins.md)
- [← Back to Cookbook](../COOKBOOK.md)
