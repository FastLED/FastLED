# FastLED RMT RX Validation Example

This example validates SPI LED output by reading back timing values using the ESP32's RMT (Remote Control) peripheral in receive mode. It performs TX→RX loopback testing to verify that transmitted LED data matches received data.

## Overview

The validation sketch tests the entire LED data transmission pipeline:
1. **Generate Test Pattern** - Create LED colors in TX buffer
2. **Start RX Reception** - Arm RMT RX channel to capture incoming signal
3. **Transmit via SPI** - Send data to LEDs using FastLED's SPI driver
4. **Capture with RMT RX** - Record timing symbols from the loopback wire
5. **Decode Symbols** - Convert RMT timing symbols back to byte data
6. **Validate** - Compare transmitted vs received LED colors

## Hardware Setup

### Required Components
- ESP32 board (any variant: ESP32, ESP32-S3, ESP32-C3, ESP32-C6)
- 1x jumper wire (short wire, <10cm recommended)
- USB cable for programming and serial monitor

### Optional Components
- WS2812B LED strip (for visual confirmation)
- 5V power supply (if using LED strip)

## Wiring Diagram

### Minimal Setup (Validation Only)

```

                 ESP32 DevKit
        ┌────────────────────────────────┐
        │                                │
        │   GPIO 5  (SPI TX) ●───────────●──┐
        │                                │  │
        │                                │ Jumper
        │                                │ (short wire)
        │   GPIO 6  (RMT RX) ●───────────●──┘
        │                                │
        │   USB ─────────────────→ Computer
        │                                │
        └────────────────────────────────┘

```

**Steps:**
1. Connect GPIO 5 to GPIO 6 with a jumper wire
2. No LED strip required for basic validation
3. Upload sketch and open Serial Monitor (115200 baud)

### Full Setup (With LED Strip)

```
ESP32 DevKit                      WS2812B LED Strip
┌─────────────────────────┐       ┌──────────────────┐
│                         │       │                  │
│  GPIO 5 (SPI TX) ───────┼───────┤ DIN (Data In)    │
│                         │   │   │                  │
│                         │   │   │ DOUT (Data Out) ─┼─┐
│  GPIO 6 (RMT RX) ───────┼───┘   │                  │ │
│                         │       │ GND ─────────────┼─┼──> GND
│  GND ───────────────────┼───────┤                  │ │
│                         │       │ 5V ──────────────┼─┼──> 5V PSU
│  USB ───── Computer     │       │                  │ │
│                         │       └──────────────────┘ │
└─────────────────────────┘                            │
                                                       │
       Loopback wire ──────────────────────────────────┘
```

**Steps:**
1. Connect GPIO 5 to LED strip Data In
2. Connect LED strip Data Out to GPIO 6 (loopback)
3. Connect LED strip GND to ESP32 GND
4. Connect LED strip 5V to external 5V power supply (DO NOT use USB power for LED strip)
5. Upload sketch and open Serial Monitor

**Warning:** LED strips draw significant current. Always use an external power supply, not USB power.

## GPIO Pin Customization

The default pins can be changed in the sketch:

```cpp
#define SPI_DATA_PIN 5       // Change to any GPIO with SPI capability
#define RMT_RX_PIN 6         // Change to any available GPIO
```

**Pin Constraints:**
- **TX pin (SPI_DATA_PIN)**: Must support SPI peripheral (most GPIOs work)
- **RX pin (RMT_RX_PIN)**: Any GPIO works (RMT RX uses GPIO matrix routing)
- **Avoid strapping pins**: GPIO 0, 2, 15 on ESP32 classic (used during boot)
- **Avoid USB pins**: GPIO 19, 20 on ESP32-S3 (if using USB)

**Common Alternatives:**
- TX: GPIO 23 (common SPI MOSI on many dev boards)
- RX: GPIO 19, 18, 21, 22 (generic GPIO pins)

## Usage

1. **Wire the hardware** - Connect GPIO 5 to GPIO 6 with jumper wire
2. **Upload sketch** - Use Arduino IDE or PlatformIO
3. **Open Serial Monitor** - Set baud rate to 115200
4. **Observe results** - Sketch runs 8 test patterns automatically

### Expected Serial Output

```
=== FastLED RMT RX Validation Sketch ===
Hardware: ESP32 (any variant)
Wiring: Connect GPIO 5 → GPIO 6 with jumper wire

Initialization complete
Starting validation tests...

=== Test 1: Solid Red ===
TX Duration: 305 us
RX Symbols: 240 (expected: 240)
Bytes Decoded: 30 (expected: 30)
Decode Errors: 0 symbols
Accuracy: 100.0% (10/10 LEDs match)
Result: PASS ✓

=== Test 2: Solid Green ===
TX Duration: 305 us
RX Symbols: 240 (expected: 240)
Bytes Decoded: 30 (expected: 30)
Decode Errors: 0 symbols
Accuracy: 100.0% (10/10 LEDs match)
Result: PASS ✓

[... additional tests ...]

=== Test Suite Complete ===
Results: 8/8 tests passed (100.0%)
Status: ALL TESTS PASSED ✓✓✓

Waiting 10 seconds before restart...
```

## Test Patterns

The sketch runs 8 different test patterns to validate various scenarios:

1. **Solid Red** - Simple single color
2. **Solid Green** - Tests different color channel
3. **Solid Blue** - Tests third color channel
4. **Rainbow Gradient** - Tests smooth color transitions
5. **Alternating R/B** - Tests rapid color changes
6. **All Off (Black)** - Tests zero values
7. **White** - Tests all channels at maximum
8. **Gradient R→B** - Tests continuous interpolation

## Troubleshooting

### Error: RX channel init failed

**Cause:** RMT RX peripheral initialization failed

**Solutions:**
- Check that you're using a supported ESP32 variant
- Verify GPIO 6 is not already in use by another peripheral
- Try a different RX pin (see GPIO Pin Customization above)

### Error: RX timeout (no signal received)

**Cause:** No signal detected on RX pin

**Solutions:**
- Verify jumper wire connection between GPIO 5 and GPIO 6
- Check that wire is making good contact (not loose)
- Ensure no other device is driving GPIO 6
- Try swapping the jumper wire (could be broken)

### Error: Decode failed (high error rate)

**Cause:** Timing symbols don't match WS2812B protocol

**Solutions:**
- Check for electromagnetic interference (keep wires short)
- Verify you're using WS2812B LEDs (not APA102 or other chipsets)
- Ensure clock resolution matches (40MHz is default)
- Try reducing LED count (fewer LEDs = fewer symbols to decode)

### MISMATCH: TX and RX colors don't match

**Cause:** Specific LED data corruption

**Solutions:**
- Check wire quality (use short, shielded wire if possible)
- Verify power supply stability
- Look for patterns (e.g., always same LED → wiring issue)
- Reduce brightness: `FastLED.setBrightness(64)` for less noise

### Tests pass but LEDs don't light up

**Cause:** LED strip not properly connected or powered

**Solutions:**
- Verify LED strip power supply (5V, sufficient amperage)
- Check LED strip Data In connects to GPIO 5 (not GPIO 6)
- Ensure LED strip GND connects to ESP32 GND (common ground)
- Test LEDs with a simpler sketch first (e.g., `examples/Blink`)

## Technical Details

### RMT RX Configuration
- **Clock Resolution**: 40 MHz (25ns per tick)
- **Buffer Size**: 1024 symbols (~42 LEDs maximum)
- **Idle Threshold**: 50μs (reset pulse detection)

### WS2812B Timing Thresholds
The decoder uses the following timing ranges (±150ns tolerance):

| Bit | High Time (T_H) | Low Time (T_L) |
|-----|----------------|----------------|
| 0   | 250-550 ns     | 700-1000 ns    |
| 1   | 650-950 ns     | 300-600 ns     |
| Reset | N/A          | >50 μs         |

### Performance Metrics
- **TX Duration**: ~30μs per LED (300μs for 10 LEDs)
- **RX Capture**: Asynchronous (non-blocking)
- **Decode Time**: <1ms for typical patterns
- **Accuracy**: >99% for clean signals

## Platform Compatibility

| Platform | Architecture | Status | Notes |
|----------|-------------|---------|-------|
| ESP32    | Xtensa      | ✅ Supported | Reference implementation |
| ESP32-S3 | Xtensa      | ✅ Supported | Recommended platform |
| ESP32-C3 | RISC-V      | ✅ Supported | Lower GPIO count |
| ESP32-C6 | RISC-V      | ✅ Supported | Latest RISC-V variant |

## Related Documentation

- **Architecture**: `docs/RMT_RX_ARCHITECTURE.md` - System design and algorithms
- **RX Channel API**: `src/platforms/esp/32/drivers/rmt_rx/rmt_rx_channel.h`
- **Decoder API**: `src/platforms/esp/32/drivers/rmt_rx/rmt_rx_decoder.h`
- **ESP-IDF RMT Documentation**: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/rmt.html

## License

This example is part of the FastLED library and follows the same license terms.
