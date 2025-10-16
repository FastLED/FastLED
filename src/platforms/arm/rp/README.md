# Raspberry Pi RP2xxx Platform Family

This directory contains FastLED support for Raspberry Pi's RP2xxx microcontroller family, which powers the Raspberry Pi Pico series of boards.

## Platform Overview

The RP2xxx family represents Raspberry Pi's custom silicon for embedded applications. All chips in this family share a common architecture based on Programmable I/O (PIO) state machines, which FastLED leverages for high-speed, precise LED control.

### Supported Platforms

| Platform | Chip | GPIO Pins | PIO Instances | CPU | Default Clock | Status |
|----------|------|-----------|---------------|-----|---------------|--------|
| **RP2040** | RP2040 | 30 (0-29) | 2 (pio0, pio1) | Dual Cortex-M0+ @ 125 MHz | 125 MHz | ✅ Supported |
| **RP2350** | RP2350 | 48 (0-47) | 3 (pio0, pio1, pio2) | Dual Cortex-M33 or RISC-V @ 150 MHz | 150 MHz | ✅ Supported |

## Directory Structure

```
rp/
├── README.md                  # This file - RP platform family overview
│
├── rpcommon/                  # Shared code for all RP2xxx platforms
│   ├── README.md              # Documentation for shared RP code
│   ├── clockless_rp_pio.h     # Common PIO-based clockless LED driver
│   ├── pio_asm.h              # PIO assembly instruction macros
│   ├── pio_gen.h              # PIO program generator for WS2812/etc
│   ├── led_sysdefs_rp_common.h # Common system definitions
│   ├── spi_hw_2_rp.cpp        # Dual-lane parallel SPI driver (2 strips)
│   ├── spi_hw_4_rp.cpp        # Quad-lane parallel SPI driver (4 strips)
│   └── spi_hw_8_rp.cpp        # Octal-lane parallel SPI driver (8 strips)
│
├── rp2040/                    # RP2040-specific code
│   ├── README.md              # RP2040 platform documentation
│   ├── fastled_arm_rp2040.h   # Main platform header
│   ├── fastpin_arm_rp2040.h   # Pin definitions (30 pins: 0-29)
│   ├── led_sysdefs_arm_rp2040.h # System defines (125 MHz default)
│   └── clockless_arm_rp2040.h # Clockless wrapper (includes rpcommon)
│
└── rp2350/                    # RP2350-specific code
    ├── README.md              # RP2350 platform documentation
    ├── fastled_arm_rp2350.h   # Main platform header
    ├── fastpin_arm_rp2350.h   # Pin definitions (48 pins: 0-47)
    ├── led_sysdefs_arm_rp2350.h # System defines (150 MHz default)
    └── clockless_arm_rp2350.h # Clockless wrapper (includes rpcommon)
```

## Key Features

### 1. PIO-Based Clockless LED Control

All RP2xxx platforms use Programmable I/O (PIO) state machines for clockless LED protocols (WS2812, WS2811, SK6812, etc.). This provides:

- **Precise Timing**: Hardware-generated waveforms with sub-microsecond accuracy
- **CPU Independence**: LEDs update in background while CPU runs other code
- **Multiple Strips**: Each PIO state machine can drive independent LED strips
- **DMA Integration**: Direct Memory Access for efficient data transfer

The PIO implementation is shared across all RP platforms in `rpcommon/clockless_rp_pio.h`.

### 2. Parallel Hardware SPI

FastLED includes specialized parallel SPI drivers that can control multiple LED strips simultaneously:

- **Dual-SPI (2 strips)**: `spi_hw_2_rp.cpp` - 2 parallel APA102/SK9822 strips
- **Quad-SPI (4 strips)**: `spi_hw_4_rp.cpp` - 4 parallel strips
- **Octal-SPI (8 strips)**: `spi_hw_8_rp.cpp` - 8 parallel strips

These drivers use PIO to generate parallel clock signals and DMA for data transfer.

### 3. Flexible Pin Mapping

All GPIO pins can be used for LED control. The RP2xxx PIO system allows any pin to be configured for input or output with hardware-timed control.

**RP2040**: Pins 0-29 (30 total)
**RP2350**: Pins 0-47 (48 total on RP2350B variant)

### 4. Fallback Cortex-M0 Driver

If PIO-based drivers are disabled or unavailable, FastLED automatically falls back to the generic Cortex-M0 bit-banging implementation from `platforms/arm/common/`.

## Platform Detection

FastLED automatically detects the platform using these preprocessor macros:

```cpp
// RP2350 detection (checked first)
#if defined(PICO_RP2350)
    #include "platforms/arm/rp/rp2350/fastled_arm_rp2350.h"

// RP2040 detection
#elif defined(ARDUINO_ARCH_RP2040) || defined(PICO_RP2040)
    #include "platforms/arm/rp/rp2040/fastled_arm_rp2040.h"
#endif
```

Platform detection is handled in `src/platforms.h`.

## Configuration Options

### PIO vs Software Bit-Banging

By default, FastLED uses PIO-based drivers for superior performance:

```cpp
// Use PIO drivers (default, recommended)
#define FASTLED_RP2040_CLOCKLESS_PIO 1

// Force software bit-banging (slower, not recommended)
#define FASTLED_RP2040_CLOCKLESS_PIO 0
```

### M0 Fallback for Compatibility

Enable the Cortex-M0 fallback driver alongside PIO:

```cpp
// Enable both PIO and M0 drivers
#define FASTLED_RP2040_CLOCKLESS_M0_FALLBACK 1
```

### Clock Speed Override

Override the default CPU frequency for timing calculations:

```cpp
// RP2040: Override default 125 MHz
#define F_CPU 133000000  // 133 MHz (common overclock)

// RP2350: Override default 150 MHz
#define F_CPU 200000000  // 200 MHz (overclocked)
```

## Code Organization Philosophy

The RP platform family uses a **shared-common + platform-specific** architecture:

### Shared Code (`rpcommon/`)

Code that works identically across all RP2xxx platforms:

- **PIO programs and macros**: Hardware timing is platform-independent
- **SPI drivers**: Use `NUM_PIOS` to adapt to platform capabilities
- **Common system defines**: Interrupt handling, delays, etc.

### Platform-Specific Code (`rp2040/`, `rp2350/`)

Code that varies between platforms:

- **Pin definitions**: Different GPIO counts (30 vs 48 pins)
- **PIO instance count**: Different `NUM_PIOS` (2 vs 3)
- **Default clock speeds**: Different F_CPU defaults (125 MHz vs 150 MHz)
- **CPU architecture**: M0+ vs M33/RISC-V (minimal impact on FastLED)

This separation ensures:
- ✅ Bug fixes benefit all platforms automatically
- ✅ Easy addition of future RP variants (e.g., RP2040B, RP3000)
- ✅ Clear boundaries between shared and unique code
- ✅ Minimal code duplication

## Adding New RP2xxx Variants

If Raspberry Pi releases new RP chips (e.g., RP2040B, RP3000), follow these steps:

1. **Create platform directory**: `src/platforms/arm/rp/rp2xxx/`
2. **Create platform headers**:
   - `fastpin_arm_rp2xxx.h` - Define GPIO pin count and pin macros
   - `led_sysdefs_arm_rp2xxx.h` - Set F_CPU default, include `rpcommon/led_sysdefs_rp_common.h`
   - `clockless_arm_rp2xxx.h` - Thin wrapper including `rpcommon/clockless_rp_pio.h`
   - `fastled_arm_rp2xxx.h` - Main header aggregating the above files
3. **Update platform detection**: Add detection macro to `src/platforms.h`
4. **Create README.md**: Document platform-specific features

The shared code in `rpcommon/` typically requires no changes.

## Hardware Boards

### RP2040-Based Boards
- Raspberry Pi Pico
- Raspberry Pi Pico W (with WiFi)
- Adafruit Feather RP2040
- SparkFun Thing Plus RP2040
- Arduino Nano RP2040 Connect
- Seeed XIAO RP2040

### RP2350-Based Boards
- Raspberry Pi Pico 2 (RP2350A: 30 GPIO, RP2350B: 48 GPIO)

## Performance Notes

### PIO Driver Performance
- **Clockless LEDs**: Up to ~4 Mbps per strip (limited by LED protocol, not RP2xxx)
- **SPI LEDs**: Up to 10-20 MHz SPI clock
- **Parallel Strips**: Limited by number of PIO state machines (4 per PIO instance)

### Recommended Settings
- **Single strip**: Any GPIO pin, uses 1 PIO state machine
- **Multiple strips**: Use different PIO instances when possible (RP2040: 2, RP2350: 3)
- **Large displays**: Consider parallel SPI drivers for 2-8 strips simultaneously

## References

- [RP2040 Datasheet](https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf)
- [RP2350 Datasheet](https://datasheets.raspberrypi.com/rp2350/rp2350-datasheet.pdf)
- [Raspberry Pi Pico C/C++ SDK](https://github.com/raspberrypi/pico-sdk)
- [RP2040 PIO Guide](https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf#pio)

## Contributing

When contributing to RP platform support:

1. **Shared code changes**: Update `rpcommon/` and test on both RP2040 and RP2350
2. **Platform-specific changes**: Only modify the relevant `rp2040/` or `rp2350/` directory
3. **Test on hardware**: Verify changes on actual boards when possible
4. **Update documentation**: Keep README.md files synchronized with code changes

---

**Last Updated**: October 2024
**Maintainer**: FastLED Project
**Status**: Production Ready
