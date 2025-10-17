# SAMD Single-SPI LED Example

This example demonstrates using FastLED with SPI-based LED chipsets on SAMD21 and SAMD51 platforms.

## Supported Platforms

| Platform | MCU | Speed | RAM | Flash | Board Examples |
|----------|-----|-------|-----|-------|----------------|
| **SAMD21** | Cortex-M0+ | 48 MHz | 32 KB | 256 KB | Arduino Zero, Adafruit Feather M0 |
| **SAMD51** | Cortex-M4F | 120 MHz | 192 KB | 512 KB | Adafruit Feather M4, Grand Central M4 |

## Supported LED Chipsets

This example works with SPI-based LED chipsets that require both DATA and CLOCK signals:

- **APA102** (DotStar) - Most common, high refresh rate
- **SK9822** - APA102 clone with improved color accuracy
- **LPD8806** - Older protocol, good for long runs
- **WS2801** - Common RGB strip controller
- **P9813** - Used in some RGB LED modules

## Hardware Connections

### Basic Wiring

```
LED Strip          SAMD Board
─────────          ──────────
DATA     ────────> MOSI pin (see pin table below)
CLOCK    ────────> SCK pin (see pin table below)
GND      ────────> GND
VCC      ────────> External 5V power supply (do NOT use board 5V for strips!)
```

**⚠️ Important Power Notes:**
- Never power LED strips directly from the board's 5V pin
- Use a separate 5V power supply rated for your strip's current draw
- Always connect grounds together (LED strip GND to board GND)
- Add a 1000 µF capacitor across LED strip power connections
- Add a 470Ω resistor in series with DATA line (optional but recommended)

### Pin Mappings (Hardware SPI)

| Board | MOSI (DATA) | SCK (CLOCK) | SERCOM Unit |
|-------|-------------|-------------|-------------|
| **Arduino Zero** | Pin 11 (ICSP) | Pin 13 (ICSP) | SERCOM1 |
| **Adafruit Feather M0** | Pin 23 | Pin 24 | SERCOM4 |
| **Adafruit Feather M4** | Pin 23 | Pin 24 | SERCOM1 |
| **Adafruit Grand Central M4** | Pin 51 (ICSP) | Pin 52 (ICSP) | SERCOM2 |

**Note:** These pins are automatically configured by the example sketch based on the detected board.

## Usage

### 1. Select Your LED Chipset

In the sketch, uncomment the line matching your LED chipset:

```cpp
// Default: APA102 (DotStar)
FastLED.addLeds<APA102, DATA_PIN, CLOCK_PIN, BGR>(leds, NUM_LEDS);

// Or choose another:
// FastLED.addLeds<SK9822, DATA_PIN, CLOCK_PIN, BGR>(leds, NUM_LEDS);
// FastLED.addLeds<LPD8806, DATA_PIN, CLOCK_PIN, RGB>(leds, NUM_LEDS);
// FastLED.addLeds<WS2801, DATA_PIN, CLOCK_PIN, RGB>(leds, NUM_LEDS);
// FastLED.addLeds<P9813, DATA_PIN, CLOCK_PIN, RGB>(leds, NUM_LEDS);
```

### 2. Adjust LED Count

Set `NUM_LEDS` to match your strip:

```cpp
#define NUM_LEDS 30  // Change to your actual LED count
```

### 3. Adjust Color Ordering (if needed)

Some LED strips have different color orderings. Try these if colors are wrong:

- `RGB` - Red, Green, Blue
- `BGR` - Blue, Green, Red (common for APA102/SK9822)
- `GRB` - Green, Red, Blue
- `RBG`, `BRG`, `GBR` - Other orderings

### 4. Compile and Upload

```bash
# Using PlatformIO
pio run -e adafruit_feather_m4 -t upload

# Or using FastLED build system
uv run ci/ci-compile.py adafruit_feather_m4 --examples SpecialDrivers/Adafruit/SAMD_SingleSPI
```

### 5. Monitor Serial Output

Open serial monitor at 115200 baud to see:
- Board detection
- Pin configuration
- FPS counter (updated every 2 seconds)

## Expected Performance

| Platform | Typical FPS | Notes |
|----------|-------------|-------|
| **SAMD51** | ~250 FPS | @ 30 LEDs, 120 MHz CPU |
| **SAMD21** | ~150 FPS | @ 30 LEDs, 48 MHz CPU |

Performance scales with LED count. More LEDs = lower FPS.

## Implementation Details

### Single-SPI Architecture

This example uses FastLED's standard single-SPI backend (`SAMDHardwareSPIOutput` class) which leverages the Arduino SPI library:

- **Initialization:** Uses `::SPI.begin()` from Arduino core
- **Transmission:** Uses `SPI.beginTransaction()` / `SPI.transfer()` / `SPI.endTransaction()`
- **Portability:** Works on all SAMD boards without custom pin configuration
- **Performance:** Hardware SPI with polling mode (non-blocking for typical use cases)

### Parallel SPI Infrastructure (Advanced)

FastLED includes infrastructure for **Dual-SPI** and **Quad-SPI** on SAMD platforms:

**Files:**
- `src/platforms/arm/d21/spi_hw_2_samd21.cpp` - SAMD21 Dual-SPI driver
- `src/platforms/arm/d51/spi_hw_2_samd51.cpp` - SAMD51 Dual-SPI driver (SERCOM-based)
- `src/platforms/arm/d51/spi_hw_4_samd51.cpp` - SAMD51 Quad-SPI driver (QSPI peripheral)
- `src/platforms/arm/sam/spi_device_proxy.h` - Device proxy for bus manager integration

**Status:**
- ✅ Low-level SERCOM/QSPI drivers fully implemented
- ✅ Bus manager integration complete
- ✅ Single-SPI backend fully functional (this example)
- ⚠️ Parallel SPI requires hardware testing and validation
- ⚠️ True multi-lane operation needs hardware-specific approach

**Recommendation:** Use single-SPI mode (this example) for production applications. Parallel SPI infrastructure is experimental and requires hardware validation.

## Troubleshooting

### LEDs don't light up

1. Check power supply (needs separate 5V source, not board power)
2. Verify ground connection between board and LED strip
3. Check DATA and CLOCK pin connections
4. Try adding 470Ω resistor in series with DATA line
5. Verify LED chipset type matches your strip
6. Check color ordering (try RGB, BGR, GRB)

### Wrong colors displayed

1. Change color ordering in `addLeds()` call (RGB, BGR, GRB, etc.)
2. Verify chipset type (APA102 vs SK9822 vs others)
3. Check if your strip requires different voltage levels (some are 3.3V only)

### Compilation errors

1. Make sure you have the correct board selected
2. Verify Adafruit SAMD board support is installed
3. Check that FastLED is using hardware SPI pins (don't use arbitrary GPIO)
4. Update to latest Arduino SAMD board definitions

### Slow frame rate

1. Reduce `NUM_LEDS` for testing
2. Remove or increase `delay(20)` in loop()
3. Verify serial output isn't slowing down loop (comment out Serial.print)
4. Check power supply can deliver enough current
5. For SAMD21, consider reducing brightness to reduce data transmission time

## Additional Resources

- **FastLED Documentation:** https://fastled.io
- **Arduino SAMD Board Support:** https://www.arduino.cc/en/Guide/ArduinoZero
- **Adafruit SAMD Boards:** https://learn.adafruit.com/adafruit-feather-m4-express-atsamd51
- **APA102/DotStar Guide:** https://learn.adafruit.com/adafruit-dotstar-leds

## Technical Notes

### SERCOM Peripheral

SAMD platforms use SERCOM peripherals (Serial Communication) that can be configured as:
- SPI (Serial Peripheral Interface)
- I2C (Inter-Integrated Circuit)
- UART (Universal Asynchronous Receiver-Transmitter)

Each board has a default SPI instance connected to specific pins. This example uses the board's default hardware SPI pins for maximum compatibility.

### Clock Speed

The SPI clock speed is automatically configured by FastLED based on the LED chipset:
- **APA102/SK9822:** Up to 24 MHz (limited by LED chipset, not MCU)
- **LPD8806:** ~2 MHz
- **WS2801:** ~1 MHz

SAMD platforms support:
- **SAMD51:** Up to 60 MHz SPI clock (MCU_FREQ / 2)
- **SAMD21:** Up to 24 MHz SPI clock (MCU_FREQ / 2)

In practice, LED chipset limits are the bottleneck, not the MCU.

### Future Enhancements

Potential improvements for SAMD parallel SPI implementation:

1. **DMA (Direct Memory Access):** Offload SPI transmission to DMA controller for true non-blocking operation
2. **Dual-SERCOM Sync:** Use two SERCOM peripherals for true 2-lane parallel operation
3. **QSPI Optimization:** Improve QSPI driver for Quad-SPI (4-lane) on SAMD51
4. **TCC + GPIO:** Use Timer/Counter for Compare (TCC) with GPIO for bit-banging additional lanes on SAMD21

These enhancements require hardware testing and validation before production use.

## License

This example is part of the FastLED library and is released under the same license.

## Contributing

Found a bug or have an improvement? Please submit an issue or pull request to the FastLED repository.
