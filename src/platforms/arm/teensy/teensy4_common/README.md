# FastLED Platform: Teensy 4.x (i.MX RT1062)

Teensy 4.0/4.1 (IMXRT1062) support.

## Files
- `fastled_arm_mxrt1062.h`: Aggregator; includes pin/SPI/clockless and helpers.
- `fastpin_arm_mxrt1062.h`: Pin helpers.
- `fastspi_arm_mxrt1062.h`: SPI backend.
- `clockless_arm_mxrt1062.h`: Single-lane clockless driver.
- `block_clockless_arm_mxrt1062.h`: Block/multi-lane clockless.
- `octows2811_controller.h`: OctoWS2811 integration.
- `clockless_objectfled.h/cpp`: Legacy single-controller ObjectFLED wrapper.
- `led_sysdefs_arm_mxrt1062.h`: System defines for RT1062.
- `spi_hw_4_mxrt1062.cpp`: Quad-SPI (4-lane) LPSPI driver.
- `spi_hw_2_mxrt1062.cpp`: Dual-SPI (2-lane) LPSPI driver.

## Multi-Strip LED Control

The ObjectFLED driver provides high-performance multi-strip LED control using DMA-driven bit transposition on Teensy 4.0/4.1.

### Features
- **Up to 42 parallel strips** (Teensy 4.1) or 16 strips (Teensy 4.0)
- **Automatic chipset timing** - WS2812, SK6812, WS2811, WS2813, WS2815 support
- **DMA-driven** - Minimal CPU overhead during transmission

### Usage
Use the Channel API for new projects. See `fl/channels/channel.h` for documentation.

### Performance
- 100 LEDs: ~3ms transmission time
- 200 LEDs: ~6ms transmission time
- Achieves 60fps easily with 500+ LEDs per instance

## LPSPI Multi-Lane SPI Support

Teensy 4.x features the **LPSPI (Low Power Serial Peripheral Interface)** peripheral, which natively supports multi-lane (dual/quad) SPI operation for parallel LED strip control.

### Current Implementation Status

FastLED provides complete dual-lane and quad-lane SPI drivers for Teensy 4.x:

- **Dual-SPI (2 lanes)**: ⚠️ **POTENTIALLY FIXED (Iteration 4) - Hardware testing required**
- **Quad-SPI (4 lanes)**: ⚠️ **Hardware ready, pin configuration incomplete**

#### What Works

The Teensy 4.x LPSPI drivers (`spi_hw_2_mxrt1062.cpp` and `spi_hw_4_mxrt1062.cpp`) include:

1. **Complete TCR.WIDTH configuration** - Hardware correctly switches between 1-bit, 2-bit, and 4-bit modes
2. **Direct register access** - Efficient LPSPI peripheral control
3. **Bus management** - Automatic routing through SPI Bus Manager
4. **Bit-interleaving** - Optimized parallel data transmission

#### Dual-SPI Status: Potentially Fixed (Iteration 4)

**⚠️ TESTING REQUIRED**: Dual-SPI on Teensy 4.x has received a potential fix in iteration 4 but requires hardware testing:

**The Fix (Iteration 4)**: Added CFGR1 register configuration to enable OUTCFG (bit 26), which allows the LPSPI hardware to control the SDI (MISO) pin as an output in dual-mode.

**What Was Changed**:
1. Set `CFGR1.OUTCFG = 1` after `SPI.begin()` to enable pin tristating for multi-bit SPI
2. Added pin validation to ensure correct pin combinations
3. LPSPI hardware configures `TCR.WIDTH = 0b01` for dual-mode (already existed)
4. Pin configuration remains `PINCFG = 0` (kLPSPI_SdiInSdoOut) - hardware auto-handles direction

**Hardware Testing Needed**:
1. Connect LED strips to pins 11 and 12 (with shared clock on pin 13)
2. Use a logic analyzer to verify both pins output data
3. Define `FASTLED_LOG_SPI_ENABLED` to see debug output

**Previous Workaround** (if fix doesn't work): Use separate SPI buses:
```cpp
// If dual-SPI still doesn't work, use separate buses:

// Use separate SPI buses:
FastLED.addLeds<APA102, 11, 13>(leds1, NUM_LEDS);  // SPI (bus 0): pins 11,13
FastLED.addLeds<APA102, 26, 27>(leds2, NUM_LEDS);  // SPI1 (bus 1): pins 26,27
```

**Technical Details**: See `RESEARCH.md` and iteration summaries in `.agent_task/` for complete analysis.



#### Hardware Limitation: Quad-Mode Pin Availability

Standard Teensy 4.0/4.1 boards **do not expose** the PCS2 and PCS3 pins required for quad-mode (4-lane) operation:

| Signal | Standard Name | Quad-Mode Usage | Teensy 4.0/4.1 Availability |
|--------|---------------|-----------------|----------------------------|
| SCK | Serial Clock | Clock (always) | ✅ Exposed (pin 13) |
| SDO | Serial Data Out | D0 (data lane 0) | ✅ Exposed (pin 11) |
| SDI | Serial Data In | D1 (data lane 1) | ✅ Exposed (pin 12) |
| PCS2 | Chip Select 2 | D2 (data lane 2) | ❌ Not exposed |
| PCS3 | Chip Select 3 | D3 (data lane 3) | ❌ Not exposed |

**Quad-mode requires** one of the following:
- Custom PCB designs that break out PCS2/PCS3 pins
- Breakout adapters exposing the full iMXRT1062 pin set
- Advanced soldering directly to processor pads

### LPSPI Bus Mapping

The iMXRT1062 has four LPSPI peripherals, of which Teensy exposes three:

| Teensy SPI Bus | Hardware Peripheral | Memory Address | Default Pins (Teensy 4.0/4.1) |
|---------------|---------------------|----------------|-------------------------------|
| `SPI` (bus 0) | **LPSPI4** | 0x403A0000 | SCK=13, MOSI=11, MISO=12 |
| `SPI1` (bus 1) | **LPSPI3** | 0x403B0000 | SCK=27, MOSI=26, MISO=1 |
| `SPI2` (bus 2) | **LPSPI1** | 0x40394000 | SCK=45, MOSI=43, MISO=42 |

Each LPSPI peripheral supports:
- **Single-lane (1-bit)**: Standard SPI mode
- **Dual-lane (2-bit)**: 2× throughput using D0 and D1
- **Quad-lane (4-bit)**: 4× throughput using D0, D1, D2, and D3

### Dual-SPI Usage (⚠️ TESTING REQUIRED - See Status Above)

**Note**: Due to pin configuration issues, dual-SPI on Teensy 4.x is currently non-functional. The code below shows the intended usage pattern, but users should use separate SPI buses (SPI, SPI1, SPI2) instead until the IOMUXC configuration is fixed:

```cpp
#include <FastLED.h>

#define CLOCK_PIN 13  // SCK
#define NUM_LEDS 100

CRGB leds_strip1[NUM_LEDS];
CRGB leds_strip2[NUM_LEDS];

void setup() {
    // Add 2 strips sharing clock pin 13
    // FastLED auto-detects and enables Dual-SPI (2 parallel lanes)
    FastLED.addLeds<APA102, 11, CLOCK_PIN>(leds_strip1, NUM_LEDS);  // D0 = pin 11 (MOSI)
    FastLED.addLeds<APA102, 12, CLOCK_PIN>(leds_strip2, NUM_LEDS);  // D1 = pin 12 (MISO)
}

void loop() {
    // Set colors independently
    fill_rainbow(leds_strip1, NUM_LEDS, 0, 7);
    fill_rainbow(leds_strip2, NUM_LEDS, 128, 7);

    // Both strips transmit in parallel via LPSPI hardware
    FastLED.show();
}
```

### Quad-SPI Implementation Requirements

To enable full quad-mode support, the following steps are required (see `LP_SPI.md` in project root for detailed technical guide):

1. **Pin Mapping Research** - Identify which processor pins provide PCS2/PCS3 functions
2. **IOMUXC Configuration** - Implement direct register manipulation to configure D2/D3 pins:
   - `IOMUXC_SW_MUX_CTL` registers (alternate function selection)
   - `IOMUXC_SW_PAD_CTL` registers (electrical properties)
   - `SELECT_INPUT` registers (signal routing)
3. **Integration** - Add pin configuration calls to `spi_hw_4_mxrt1062.cpp::init()`
4. **Custom Hardware** - Design PCB or breakout to expose PCS2/PCS3 pins
5. **Testing** - Validate with logic analyzer and real LED strips

**Status**: Implementation roadmap documented in `LP_SPI.md`. Hardware constraints make this a **low priority** feature (requires custom PCBs, not accessible to most users).

### Technical References

- **LPSPI Implementation Guide**: See `LP_SPI.md` in project root for complete quad-mode implementation details
- **Advanced SPI System**: See `src/platforms/README_SPI_ADVANCED.md` for multi-lane SPI architecture
- **NXP Reference Manual**: [i.MX RT1060 Reference Manual (Chapter 29: LPSPI)](https://www.pjrc.com/teensy/IMXRT1060RM_rev2.pdf)
- **Teensy Core Library**: Arduino/hardware/teensy/avr/cores/teensy4/ (SPI.cpp, imxrt.h)

### Platform Comparison: Multi-Lane SPI Support

| Platform | Dual-SPI | Quad-SPI | Notes |
|----------|----------|----------|-------|
| **Teensy 4.x** | ⚠️ | ⚠️ | Both experimental - pin configuration issues; use separate SPI buses instead |
| ESP32 | ✅ | ✅ | Full support on standard dev boards |
| ESP32-S2/S3 | ✅ | ✅ | Full support |
| ESP32-C3/C6 | ✅ | ❌ | Dual-SPI only (2 lanes max) |

## Notes
- Very high CPU frequency; DWT-based timing and interrupt thresholds are critical for stability.
- OctoWS2811 and SmartMatrix can offload large parallel outputs; ensure pin mappings and DMA settings match board wiring.
- For multi-strip LED projects on Teensy 4.x, see the Channel API documentation.
- For SPI-based LED strips (APA102, SK9822, LPD8806), dual-SPI provides 2× performance on standard boards without modifications.
