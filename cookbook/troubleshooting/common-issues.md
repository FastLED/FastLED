# Common Issues and Solutions

**Difficulty Level**: ⭐ Beginner
**Time to Complete**: 5-10 minutes
**Prerequisites**:
- [Getting Started: First Example](../getting-started/first-example.md) - Basic FastLED setup
- [Getting Started: Hardware](../getting-started/hardware.md) - Hardware wiring basics

**You'll Learn**:
- How to diagnose and fix the most common LED problems (no lights, wrong colors, flickering)
- Quick solutions for power and wiring issues that affect 80% of LED problems
- How to identify signal integrity issues (wrong first LED, data connection problems)
- When to adjust software settings (brightness, color order, power limits) vs hardware fixes
- Which advanced troubleshooting guide to consult when quick fixes don't resolve the problem

---

Quick reference guide for the most frequently encountered FastLED problems and their solutions.

## LEDs Don't Light Up

**Possible causes and solutions:**

- **Check power connections (GND and 5V)**
  - Verify both ground and 5V are securely connected to the LED strip
  - Ensure power supply is plugged in and turned on
  - Test power supply with a multimeter (should read ~5V)

- **Verify data pin matches code**
  - Double-check that `LED_PIN` in your code matches the physical pin
  - Arduino pin numbering can vary by board
  - Try a different pin to rule out damaged GPIO

- **Try reducing brightness**
  ```cpp
  FastLED.setBrightness(50);  // Start with low brightness
  ```
  - Some power supplies can't handle full brightness
  - Helps isolate power vs. data issues

- **Check LED type and color order match your strip**
  ```cpp
  // Make sure these match your actual hardware
  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
  ```
  - WS2812B is most common, but verify your strip type
  - See [Color Order Issues](#wrong-colors) below

## Wrong Colors

**Symptoms:** LEDs light up but show incorrect colors (e.g., red appears green)

**Solution:** Try different COLOR_ORDER values

```cpp
// Try each of these in order until colors are correct:
FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);  // Most common
FastLED.addLeds<WS2812B, LED_PIN, RGB>(leds, NUM_LEDS);
FastLED.addLeds<WS2812B, LED_PIN, BGR>(leds, NUM_LEDS);
FastLED.addLeds<WS2812B, LED_PIN, BRG>(leds, NUM_LEDS);
FastLED.addLeds<WS2812B, LED_PIN, RBG>(leds, NUM_LEDS);
FastLED.addLeds<WS2812B, LED_PIN, GBR>(leds, NUM_LEDS);
```

**Testing color order:**
```cpp
void testColorOrder() {
    leds[0] = CRGB(255, 0, 0);  // Should show RED
    leds[1] = CRGB(0, 255, 0);  // Should show GREEN
    leds[2] = CRGB(0, 0, 255);  // Should show BLUE
    FastLED.show();
}
```

**Common for WS2812B:** Use `GRB` (not RGB)

## Flickering

**Symptoms:** LEDs flicker, flash randomly, or show unstable colors

**Solutions:**

1. **Add capacitor across power supply**
   - Use 1000µF or larger electrolytic capacitor
   - Connect between 5V and GND at power supply
   - Helps smooth power delivery

2. **Ensure solid ground connection**
   - Microcontroller GND must connect to LED strip GND
   - Use thick wire for ground connection
   - Poor ground is a very common cause of flickering

3. **Check power supply capacity**
   - Calculate required current: `NUM_LEDS × 60mA`
   - Example: 100 LEDs = 6A minimum
   - Use power supply rated 20% higher than calculated need
   - Underpowered supplies cause brownouts and flickering

4. **Software fixes**
   ```cpp
   // Limit power draw
   FastLED.setMaxPowerInVoltsAndMilliamps(5, 2000);

   // Reduce brightness
   FastLED.setBrightness(100);  // Instead of 255
   ```

## First LED Wrong, Rest OK

**Symptoms:** First LED shows wrong color or behavior, but remaining LEDs work correctly

**Cause:** Signal integrity issue - first LED is most sensitive to data signal quality

**Solutions:**

1. **Add 220-470Ω resistor on data line**
   - Place resistor between microcontroller pin and LED strip DATA
   - Resistor should be physically close to microcontroller
   - This is the most effective fix

2. **Keep data wire short**
   - Less than 6 inches if possible
   - Longer wires pick up more interference

3. **Use level shifter for 3.3V to 5V conversion**
   - Required for 3.3V microcontrollers (ESP8266, some ESP32 boards)
   - 74HCT245 or dedicated level shifter modules work well

4. **Check data pin connection**
   - Ensure wire is making good contact
   - Try resoldering connection
   - Test with different data pin

## Additional Quick Fixes

### LEDs Light Up Briefly Then Turn Off
- Power supply overload protection activating
- Solution: Use larger power supply or reduce LED count/brightness

### Only Some LEDs Work
- Data signal degradation along strip
- Solutions: See [Hardware Problems - Strip Works Partially](hardware.md#strip-works-partially)

### Random Colors at Power On
- Normal behavior - LEDs contain random data before `FastLED.show()` called
- Solution: Call `FastLED.clear()` or `fill_solid(leds, NUM_LEDS, CRGB::Black)` in `setup()`

### Colors Change When Touching Wires
- Poor connection or ground issue
- Solutions: Resolder connections, ensure shared ground, add capacitor

## See Also

- [Debugging Tips](debugging.md) - Diagnostic code and techniques
- [Hardware Problems](hardware.md) - In-depth hardware troubleshooting
- [Main Cookbook](../COOKBOOK.md) - Full FastLED guide
