# ESP32 SPI Driver for FastLED

This directory contains the ESP32-specific implementation of FastLED's SPI channel engine for LED control.

## Quick Reference

**Main Documentation**: See `src/platforms/README_SPI_ADVANCED.md` for comprehensive SPI system architecture.

## ESP32 Platform SPI Peripheral Availability

| Platform | SPI0 | SPI1 | SPI2 | SPI3 | Available for LEDs | FastLED Status |
|----------|------|------|------|------|-------------------|----------------|
| **ESP32 (classic)** | Flash cache | Flash | ✅ General | ✅ General | **2 hosts** (SPI2+SPI3) | ✅ **Enabled** |
| **ESP32-S2** | Flash cache | Flash | ✅ General | ✅ General | **2 hosts** (SPI2+SPI3) | ✅ **Enabled** |
| **ESP32-S3** | Flash cache | Flash | ✅ General | ✅ General | **2 hosts** (SPI2+SPI3) | ✅ **Enabled** |
| **ESP32-C3** | Flash cache | Flash | ✅ General | ❌ N/A | **1 host** (SPI2) | ✅ **Enabled** (limited) |
| **ESP32-C6** | Flash cache | Flash | ✅ General | ❌ N/A | **1 host** (SPI2) | ❌ **Disabled** (RMT5 used) |
| **ESP32-H2** | Flash cache | Flash | ✅ General | ❌ N/A | **1 host** (SPI2) | ❌ **Disabled** (RMT5 used) |
| **ESP32-P4** | Flash cache | Flash | ✅ General | ✅ General | **2 hosts** (SPI2+SPI3) | ✅ **Enabled** + Octal |

### Key Facts

1. **SPI0/SPI1 are ALWAYS reserved** for flash/PSRAM operations on all ESP32 variants
2. **SPI2/SPI3** are general-purpose SPI hosts available for peripherals like LEDs
3. **ESP-IDF explicitly prohibits** using SPI0/SPI1 via `spi_bus_initialize()` API

### ESP32-C6 and ESP32-H2 - Why No SPI Engine?

**Answer**: These chips **DO have SPI2 available**, but FastLED intentionally doesn't use it:

**Reasons:**
- **Limited scalability**: Only 1 SPI host (vs 2-3 on other ESP32 chips)
- **Better alternative**: RMT5 provides superior performance with nanosecond-precise timing
- **Preserves resources**: Leaves SPI2 available for other user peripherals
- **Simplified architecture**: RMT5-only chips use a single, optimized code path

**Misconception**: Earlier comments stated "ESP32-C6 does not have available SPI hosts (max 0 hosts)" - this was **imprecise**. The accurate statement is: "ESP32-C6 has 1 SPI host (SPI2), but RMT5 is the preferred engine for LED control."

## File Overview

| File | Purpose |
|------|---------|
| `channel_engine_spi.cpp` | Main SPI channel engine implementation (900+ lines) |
| `channel_engine_spi.h` | Generic SPI channel interface (480 lines) |
| `spi_hw_1_esp32.cpp` | Single-lane SPI hardware driver |
| `spi_esp32_init.cpp` | ESP32 SPI initialization and configuration |
| `spi_platform_esp32.cpp` | Platform-specific SPI utilities |
| `spi_device_proxy.h` | Template proxy for routing SPI calls |
| `idf5_clockless_spi_esp32.h` | ESP-IDF 5.x SPI configuration |

## Architecture

```
User API (FastLED.addLeds<APA102, DATA, CLK>)
  ↓
ChannelEngineSpi (this directory)
  ↓
SPI Bus Manager (acquires SPI2/SPI3 hosts)
  ↓
ESP32 SPI Peripheral (hardware DMA)
```

## Supported LED Chipsets

SPI-based LED chipsets that work with this engine:

- **APA102** (4-wire: Clock + Data)
- **SK9822** (APA102 compatible)
- **LPD8806** (3-wire SPI)
- **WS2801** (3-wire SPI)
- **P9813** (4-wire SPI)

## Multi-Lane Support

### Single-Lane SPI (Standard)
- Uses MOSI (data) + CLK (clock)
- 1 LED strip per SPI host
- Platforms: All ESP32 variants

### Dual-Lane SPI (2 parallel strips)
- Uses MOSI + MISO (2 data lines) + CLK
- 2 LED strips transmit simultaneously
- Platforms: All ESP32 variants with SPI engine enabled

### Quad-Lane SPI (4 parallel strips)
- Uses MOSI + MISO + WP + HD (4 data lines) + CLK
- 4 LED strips transmit simultaneously
- Platforms: ESP32, ESP32-S2, ESP32-S3 only
- **NOT supported**: ESP32-C3, ESP32-C6, ESP32-H2 (hardware limitation)

## SPI Host Acquisition Priority

**Code**: `channel_engine_spi.cpp:525-538`

```cpp
spi_host_device_t hosts[] = {
#ifdef SPI2_HOST
    SPI2_HOST,  // Priority 1: HSPI/SPI2
#endif
#ifdef SPI3_HOST
    SPI3_HOST,  // Priority 2: VSPI/SPI3
#endif
#ifdef SPI1_HOST
    SPI1_HOST,  // Priority 3: Flash SPI (avoid, last resort)
#endif
};
```

**Note**: SPI1 is included with lowest priority but is typically reserved for flash. In practice, ESP32 variants use SPI2 and SPI3 for LED control.

## Configuration

### Clock Speed

Default: 2.5 MHz for WS2812-over-SPI encoding

**Code**: `channel_engine_spi.cpp`

```cpp
#define FASTLED_SPI_CLOCKLESS_DEFAULT_FREQ_MHZ 2.5
```

### WS2812-over-SPI Encoding

- **Bit expansion**: 3x (each LED bit → 3 SPI bits)
- **Encoding**: `100` for bit 0, `110` for bit 1 (MSB first)
- **Timing**: Precise timing via SPI clock frequency

### Wave8 Encoding (New in v6.0)

The SPI engine uses **wave8 encoding**, a high-performance lookup-table-based bit expansion system that replaces the previous bit-by-bit encoding:

**Key Features**:
- **8x expansion ratio**: Each LED byte → 8 SPI bytes (configurable via LUT)
- **Lookup table**: Pre-computed nibble-to-wave8 mapping cached per channel
- **Zero-copy**: Encodes directly into DMA staging buffers (ISR-safe)
- **Multi-lane support**: Automatic transposition for dual/quad-lane modes

**Performance**:
- **~20x faster** than bit-by-bit encoding during ISR preparation
- **Memory overhead**: +64 bytes per SPI channel (LUT cache)
- **ISR overhead**: ~10 CPU cycles per input byte (negligible)

**Architecture Alignment**: Wave8 encoding matches the PARLIO engine architecture, providing consistent encoding behavior across ESP32 LED drivers.

**Implementation Details**:
- Single-lane: Direct wave8 conversion (no transposition)
- Dual-lane: Wave8 + 2-lane bit transposition (inline de-interleaving)
- Quad-lane: Wave8 + 4-lane bit transposition (inline de-interleaving)

**See**: `src/platforms/esp/32/drivers/spi/wave8_encoder_spi.{h,cpp}` for implementation details.

## Performance

**ESP32 @ 240 MHz, 40 MHz SPI clock, 100 LEDs**

| Configuration | Time | CPU Usage | Speedup |
|---------------|------|-----------|---------|
| Software bit-bang | 2.0 ms | 100% | 1x baseline |
| Single-lane SPI | 2.0 ms | 0% | ~1x (DMA) |
| Dual-lane SPI (2 strips) | 1.0 ms | 0% | 2x |
| Quad-lane SPI (4 strips) | 0.5 ms | 0% | 4x |

**Key Advantage**: DMA-driven transmission = **0% CPU usage** during LED updates

## Debugging

Enable debug output:

```cpp
#define FASTLED_DEBUG 1
#include <FastLED.h>
```

Debug messages include:
- SPI host acquisition
- Bus initialization
- DMA buffer allocation
- Transmission timing

## Known Limitations

1. **ESP32-C6/H2**: SPI engine disabled (see explanation above)
2. **PARLIO on ESP32-C6**: Clock gating bug in ESP-IDF ≤ v5.4.1 (may be fixed in future releases)
3. **SPI1 usage**: Generally avoided due to flash conflict
4. **Quad-SPI on RISC-V**: ESP32-C3/C6/H2 support dual-lane max (hardware limitation)

## Related Documentation

- **Main SPI Documentation**: `src/platforms/README_SPI_ADVANCED.md` (comprehensive guide)
- **Channel Bus Manager**: `src/platforms/esp/32/drivers/channel_bus_manager_esp32.cpp`
- **Platform Detection**: `src/platforms/esp/is_esp.h`
- **Feature Flags**: `src/platforms/esp/32/feature_flags/enabled.h`

## References

- [ESP-IDF SPI Master Driver Documentation](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/spi_master.html)
- [ESP32 Technical Reference Manual](https://www.espressif.com/sites/default/files/documentation/esp32_technical_reference_manual_en.pdf)
- [ESP32-C6 Technical Reference Manual](https://cdn.sparkfun.com/assets/2/0/b/d/8/ESP32-C6_Technical_Reference_Manual.pdf)
