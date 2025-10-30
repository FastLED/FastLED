# Power Considerations

**Difficulty Level**: ⭐⭐ Beginner to Intermediate
**Time to Complete**: 30-40 minutes
**Prerequisites**:
- [Getting Started - Basic Concepts](../getting-started/concepts.md)
- [Getting Started - Hardware Setup](../getting-started/hardware.md)

**You'll Learn**:
- How to calculate power requirements for your LED projects
- How to choose the right power supply and avoid safety hazards
- How to reduce power consumption using software controls
- How to handle voltage drop and power injection for large installations
- How to safely wire power supplies and avoid damaging your components

Manage power consumption safely in your LED projects.

## Current Requirements

### Rule of Thumb

**60mA per LED at full white brightness**

This is the maximum current draw when all three color channels (R, G, B) are at 255.

### Calculate Power Needs

```
Total current = Number of LEDs × 60mA

Examples:
- 30 LEDs:  30 × 60mA = 1.8A  (recommend 2.5A supply)
- 60 LEDs:  60 × 60mA = 3.6A  (recommend 5A supply)
- 100 LEDs: 100 × 60mA = 6.0A (recommend 8A supply)
- 200 LEDs: 200 × 60mA = 12A  (recommend 15A supply)
```

**Always add 20-30% margin** to account for power supply aging and voltage regulation.

## Power Supply Selection

| LED Count | Max Current | Recommended Supply | Voltage |
|-----------|-------------|-------------------|---------|
| 30        | 1.8A        | 2.5A              | 5V      |
| 60        | 3.6A        | 5A                | 5V      |
| 100       | 6.0A        | 8A                | 5V      |
| 150       | 9.0A        | 12A               | 5V      |
| 200       | 12.0A       | 15A               | 5V      |

**Note**: Most LED strips use 5V. Some specialized strips use 12V - check your datasheet.

## Reducing Power Draw

### Method 1: Lower Brightness

```cpp
void setup() {
    FastLED.addLeds<WS2812B, DATA_PIN>(leds, NUM_LEDS);
    FastLED.setBrightness(50);  // ~20% brightness = ~20% power
}
```

Brightness scales linearly with power consumption.

### Method 2: Set Power Limit

FastLED can automatically limit power draw:

```cpp
void setup() {
    FastLED.addLeds<WS2812B, DATA_PIN>(leds, NUM_LEDS);

    // Limit to 5V, 2000mA (2A)
    FastLED.setMaxPowerInVoltsAndMilliamps(5, 2000);
}
```

This automatically reduces brightness when needed to stay within power limits.

### Method 3: Limit Pattern Complexity

```cpp
// HIGH POWER: All LEDs full white
fill_solid(leds, NUM_LEDS, CRGB::White);  // 60mA per LED

// MEDIUM POWER: Single color
fill_solid(leds, NUM_LEDS, CRGB::Red);    // ~20mA per LED

// LOW POWER: Dim colors
fill_solid(leds, NUM_LEDS, CRGB(50, 50, 50));  // ~10mA per LED
```

### Method 4: Use Fewer LEDs at Full Brightness

```cpp
void chase() {
    // Only 1 LED is bright at a time
    FastLED.clear();
    leds[position] = CRGB::White;
    FastLED.show();
}
```

## Real-World Power Usage

Most patterns use **30-50% of maximum power** because:
- Not all LEDs are on at once
- Not all LEDs are at full brightness
- Colors aren't usually pure white

### Typical Power Usage by Pattern

| Pattern Type        | Approximate Power | Example                  |
|---------------------|-------------------|--------------------------|
| Chase/Scanner       | 10-20%            | Single LED moving        |
| Rainbow             | 40-60%            | Full strip, varied colors|
| Fire effect         | 30-50%            | Oranges and reds         |
| White strobe        | 90-100%           | All LEDs full white      |
| Twinkle             | 20-40%            | Sparse LEDs on           |

## Power Injection

For strips with 100+ LEDs, inject power at multiple points:

```
[Power Supply]
     |
     +---> Strip Beginning (VCC, GND)
     |
     +---> Strip Middle (VCC, GND)
     |
     +---> Strip End (VCC, GND)
```

**Why?** Voltage drop along the strip causes:
- Dimmer LEDs at the end
- Color shift (not enough voltage for all channels)
- Unstable operation

## Voltage Drop

LED strips have resistance. Over distance, voltage drops:

```
At start of strip:    5.0V
After 60 LEDs:        4.7V
After 100 LEDs:       4.4V  (too low - colors shift)
After 150 LEDs:       4.0V  (unreliable)
```

**Solution**: Inject power every 60-100 LEDs.

## Power Injection Example

```cpp
// For 200 LED strip with power injection at LED 0, 100, 200

void setup() {
    FastLED.addLeds<WS2812B, DATA_PIN>(leds, NUM_LEDS);

    // Still limit total power
    FastLED.setMaxPowerInVoltsAndMilliamps(5, 10000);  // 10A total

    // Wire: Power supply to strip at positions 0, 100, 200
}
```

## Capacitors

### Power Supply Capacitor

**Always** add a large capacitor across power supply:

```
1000µF (or larger) capacitor between 5V and GND
```

- Smooths voltage fluctuations
- Protects against power spikes during LED updates
- Essential for stable operation

### Per-Section Capacitors

For large installations, add capacitors at each power injection point:

```
470-1000µF capacitor at each power connection
```

## Safety Considerations

### Don't Power LEDs from Microcontroller

```cpp
// DANGEROUS: Arduino 5V pin can supply ~500mA max
// This will damage the Arduino or crash it

// SAFE: Use external power supply
```

**Never** power more than ~5-10 LEDs from the microcontroller's power pins.

### Shared Ground

**Critical**: Microcontroller and LED power supply **must share ground**:

```
[Power Supply GND] ---+--- [LED Strip GND]
                      |
                      +--- [Microcontroller GND]
```

Without shared ground:
- Erratic LED behavior
- Data corruption
- Potential damage to components

### Current Limits by Power Source

| Source               | Max Current | Safe LED Count |
|----------------------|-------------|----------------|
| Arduino USB          | 500mA       | 5-8 LEDs       |
| Arduino 5V pin       | 500mA       | 5-8 LEDs       |
| USB power bank       | 1-2A        | 15-30 LEDs     |
| Wall adapter (5V, 2A)| 2A          | 30 LEDs        |
| Dedicated LED PSU    | 5-30A       | 80+ LEDs       |

## Measuring Power

### Using a Multimeter

1. Set multimeter to DC current mode (10A range)
2. Connect in series between power supply and LED strip
3. Run your pattern
4. Read actual current draw

### Estimating in Code

FastLED can estimate power draw:

```cpp
void setup() {
    FastLED.setMaxPowerInVoltsAndMilliamps(5, 2000);
}

void loop() {
    updateLEDs();
    FastLED.show();

    // Check estimated power usage
    // (requires power calculation enabled)
}
```

## Best Practices

### Start Conservative

```cpp
void setup() {
    FastLED.addLeds<WS2812B, DATA_PIN>(leds, NUM_LEDS);

    // Start with low brightness while testing
    FastLED.setBrightness(30);

    // Set power limit
    FastLED.setMaxPowerInVoltsAndMilliamps(5, 2000);
}
```

### Scale Up Gradually

1. Test with 10 LEDs first
2. Verify power supply can handle current
3. Add more LEDs in sections
4. Monitor voltage at far end of strip
5. Add power injection as needed

### Monitor Temperature

Power supplies and LED strips generate heat:
- Power supply should feel warm, not hot
- LED strip should be touchable
- If too hot, reduce brightness or current

## Example Power-Safe Setup

```cpp
#define LED_PIN     5
#define NUM_LEDS    100
#define MAX_POWER_MILLIAMPS 4000  // 4A power supply

CRGB leds[NUM_LEDS];

void setup() {
    FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);

    // Set reasonable brightness
    FastLED.setBrightness(80);  // ~30% brightness

    // Enable automatic power limiting
    FastLED.setMaxPowerInVoltsAndMilliamps(5, MAX_POWER_MILLIAMPS);
}

void loop() {
    // Your pattern here
    rainbowEffect();
    FastLED.show();

    delay(20);
}
```

## Troubleshooting Power Issues

### LEDs flicker or turn off randomly
- Insufficient power supply current
- Poor connections
- Missing capacitor

### Colors wrong at end of strip
- Voltage drop - add power injection
- Power supply voltage too low

### Microcontroller resets
- Power supply overload
- Missing shared ground
- Capacitor needed

### First few LEDs different colors
- Data signal issue (not power)
- Add resistor on data line

## Next Steps

- [Basic Patterns](../basic-patterns/) - Create power-efficient effects
- [Troubleshooting](../troubleshooting/hardware.md) - Fix power-related issues
