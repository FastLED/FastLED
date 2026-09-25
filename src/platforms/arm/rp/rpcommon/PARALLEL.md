# Automatic Parallel Output for RP2040/RP2350

## Overview

The automatic parallel output driver enables **seamless parallel LED control** on RP2040/RP2350 platforms using the standard FastLED API. Legacy `addLeds<>()` strips are routed through `fl::SlimBridgeController` onto the shared `ChannelEngineRpPio`, which:

- ✅ Works with standard `FastLED.addLeds()` calls
- ✅ Automatically detects consecutive GPIO pins
- ✅ Groups them for efficient parallel output (2, 4, or 8 pins)
- ✅ Runs non-consecutive (independent) strips concurrently on their own lanes (#4620)
- ✅ Uses the same `ChannelEngineRpPio` (PIO0) owner as the default path and the Channel API

`FASTLED_RP2040_CLOCKLESS_PIO_AUTO` maps WS2812 `addLeds<>()` onto the same slim bridge as the default path. Consecutive pins with matching LED count and timing are batched into one multi-lane state machine. The old `RectangularDrawBuffer` group manager and CPU transpose step have been removed.

With `FASTLED_RP2040_CLOCKLESS_PIO=0`, clockless strips instead go through `fl::SlimBridgeController` onto the blocking bit-bang engine `ChannelEngineRpBitBang` (`Bus::BIT_BANG`). Interrupts are disabled for each frame.

## Quick Start

### Enable the Driver

Define `FASTLED_RP2040_CLOCKLESS_PIO_AUTO` before including FastLED.h:

```cpp
#define FASTLED_RP2040_CLOCKLESS_PIO_AUTO 1
#include <FastLED.h>

#define NUM_LEDS 100

// Standard FastLED arrays
CRGB leds1[NUM_LEDS];
CRGB leds2[NUM_LEDS];
CRGB leds3[NUM_LEDS];
CRGB leds4[NUM_LEDS];

void setup() {
    // Just use standard addLeds() - automatic parallel grouping!
    FastLED.addLeds<WS2812, 2, GRB>(leds1, NUM_LEDS);  // GPIO 2
    FastLED.addLeds<WS2812, 3, GRB>(leds2, NUM_LEDS);  // GPIO 3 (grouped with 2)
    FastLED.addLeds<WS2812, 4, GRB>(leds3, NUM_LEDS);  // GPIO 4 (grouped with 2-3)
    FastLED.addLeds<WS2812, 5, GRB>(leds4, NUM_LEDS);  // GPIO 5 (grouped with 2-4)
}

void loop() {
    // Update LEDs
    fill_rainbow(leds1, NUM_LEDS, millis() / 10);
    // Standard FastLED.show() - all 4 strips output in parallel!
    FastLED.show();
}
```

## How It Works

### Automatic Pin Grouping

When `FastLED.show()` is called, the driver:

1. **Collects all registered pins** from `addLeds()` calls
2. **Sorts pins numerically** (e.g., [5, 2, 4, 3] → [2, 3, 4, 5])
3. **Detects consecutive runs**:
   - [2, 3, 4, 5] → Single 4-pin group
   - [2, 3, 5, 6] → Two 2-pin groups
   - [2, 5, 10] → Three independent single-lane strips (still concurrent)
4. **Batches** consecutive pins with matching length and timing into one multi-lane PIO state machine
5. **Outputs all lanes** through `ChannelEngineRpPio` on PIO0

### Parallel Group Sizes

| Consecutive Pins | Group Size | PIO Output Mode |
|------------------|------------|-----------------|
| 2 pins | 2-lane parallel | Multi-lane SM |
| 3 pins | 3-lane batch | Multi-lane SM |
| 4 pins | 4-lane parallel | Multi-lane SM |
| 5-7 pins | 5- to 7-lane batch | Multi-lane SM |
| 8+ pins | 8-lane parallel | Multi-lane SM |

Non-consecutive pins become independent lanes that still run concurrently (#4620).

### Example: Mixed Groups

```cpp
// Pins: 2, 3, 4, 5, 10, 11, 15
FastLED.addLeds<WS2812, 2>(...);  // ┐
FastLED.addLeds<WS2812, 3>(...);  // ├─ Group 1: 4-lane parallel (GPIO 2-5)
FastLED.addLeds<WS2812, 4>(...);  // │
FastLED.addLeds<WS2812, 5>(...);  // ┘
FastLED.addLeds<WS2812, 10>(...); // ┬─ Group 2: 2-lane parallel (GPIO 10-11)
FastLED.addLeds<WS2812, 11>(...); // ┘
FastLED.addLeds<WS2812, 15>(...); // ── Group 3: Sequential (GPIO 15 alone)
```

**Resources Used:**
- Group 1 (4-pin): 1 PIO state machine + 1 DMA channel
- Group 2 (2-pin): 1 PIO state machine + 1 DMA channel
- Group 3 (1-pin): Uses the same PIO/DMA engine in single-lane mode

## Hardware Requirements

### Pin Consecutiveness (Critical!)

**RP2040/RP2350 PIO hardware requires consecutive GPIO pins for parallel output.**

This is a hardware limitation of the PIO `out pins, N` instruction.

✅ **Valid Configurations:**
```
GPIO 2-3   (2 pins)
GPIO 2-5   (4 pins)
GPIO 10-17 (8 pins)
GPIO 0-7   (8 pins)
```

❌ **Invalid Configurations:**
```
GPIO 2, 4, 6, 8  (non-consecutive - independent lanes, run concurrently)
GPIO 1, 3, 5     (non-consecutive - independent lanes, run concurrently)
```

### GPIO Pin Availability

- **RP2040**: 30 GPIO pins (GPIO 0-29)
- **RP2350**: 48 GPIO pins (GPIO 0-47, some reserved)

**Avoid using:**
- GPIO 19-20 (USB UART on some boards)
- GPIO 23-25 (SPI flash on Pico)
- Pins used for I2C, SPI, UART if needed

**Recommended consecutive ranges:**
- GPIO 2-9 (8 consecutive pins)
- GPIO 10-17 (8 consecutive pins)
- GPIO 20-27 (8 consecutive pins, avoid if using USB UART)

## Performance

### Lane Packing

`ChannelEngineRpPio` packs lane data for its multi-lane state machine internally. The legacy CPU transpose (`parallel_transpose.h`) no longer exists.

### Frame Rate

**WS2812B timing limits:**
- 100 LEDs × 4 strips = ~12 ms per frame
- **Maximum ~83 FPS** (timing limited, not CPU limited)

**Practical frame rates:**
- 50 FPS: 20 ms delay (recommended)
- 60 FPS: 16.7 ms delay
- 83 FPS: No delay (WS2812 timing limit)

### Memory Usage

Pixel buffers are owned by the channel engine: roughly one encoded buffer per multi-lane batch. There is no separate `RectangularDrawBuffer` or transpose buffer any more.

**Memory location:**
- RP2040: Main SRAM (264 KB)
- RP2350: Main SRAM (520 KB)
- No PSRAM support (unlike ESP32-S3)

### Resource Usage

**Per parallel group:**
- 1 PIO state machine (from 8 available: 2 PIOs × 4 SMs)
- 1 DMA channel (from 12 available)
- ~32 PIO instructions for timing program

**Example: 4-pin + 2-pin + 1-pin groups:**
- 2 PIO state machines for the 4-pin and 2-pin batches
- 2 DMA channels
- plus 1 more SM + DMA channel for the 1-pin lane

## Usage

```cpp
#define FASTLED_RP2040_CLOCKLESS_PIO_AUTO 1
#include <FastLED.h>

CRGB leds1[100], leds2[100], leds3[100], leds4[100];

void setup() {
    FastLED.addLeds<WS2812, 2, GRB>(leds1, 100);
    FastLED.addLeds<WS2812, 3, GRB>(leds2, 100);
    FastLED.addLeds<WS2812, 4, GRB>(leds3, 100);
    FastLED.addLeds<WS2812, 5, GRB>(leds4, 100);
}

void loop() {
    FastLED.show();  // Standard API!
}
```

**Benefits:**
- ✅ Standard FastLED API (no custom classes)
- ✅ Works with all FastLED features (brightness, color correction, etc.)
- ✅ Automatic grouping detection
- ✅ Graceful fallback for non-consecutive pins

## Limitations

### Hardware Constraints

1. **Consecutive pins required** for parallel output
   - Hardware limitation of PIO `out pins, N` instruction
   - Non-consecutive pins run as independent concurrent lanes

2. **Maximum 12 DMA channels** (shared with other peripherals)
   - Each parallel group uses 1 DMA channel
   - Sequential groups may share resources

3. **Maximum 8 PIO state machines** (2 PIOs × 4 SMs)
   - Each parallel group uses 1 SM
   - Shared with other PIO-based features (SPI, I2C, etc.)

### Software Constraints

1. **Variable strip lengths supported** but padded to maximum
   - If strips have different LED counts, all are padded to the longest
   - Example: [50, 100, 75] LEDs → all padded to 100 LEDs internally

2. **RGBW mode fully supported** ✅
   - RGBW uses 4 bytes per LED vs 3 for RGB
   - Buffer sizes increase accordingly (4 bytes vs 3 bytes per LED)
   - Mixed RGB/RGBW strips in same parallel group supported (see RGBW section below)

3. **Sequential fallback** uses the same timing-safe PIO/DMA engine in
   single-lane mode, so mixed consecutive/non-consecutive layouts remain fully
   functional.

## Troubleshooting

### "Failed to claim PIO state machine"

**Cause:** All 8 PIO state machines are in use by other peripherals.

**Solutions:**
- Reduce number of parallel groups (use fewer consecutive pins)
- Disable other PIO-based features (e.g., parallel SPI)
- Batch more strips onto consecutive pins

### "Failed to claim DMA channel"

**Cause:** All 12 DMA channels are in use.

**Solutions:**
- Reduce number of parallel groups
- Disable other DMA-based features
- Batch more strips onto consecutive pins

### Strips flicker or show wrong colors

**Possible causes:**
1. **Power supply issues** (most common)
   - Ensure adequate current capacity (60 mA per LED max)
   - Use external 5V power supply for >50 LEDs
   - Add bypass capacitors (100-1000 µF)

2. **GPIO pin conflict**
   - Check that pins aren't used by other peripherals
   - Avoid GPIO 19-20 (USB UART) if using Serial

3. **Timing issues**
   - WS2812B requires precise timing (handled by PIO)
   - Ensure no interrupt-heavy code in loop()

## Advanced Usage

### Dynamic Strip Addition

Strips can be added at runtime:

```cpp
void setup() {
    // Initial strips
    FastLED.addLeds<WS2812, 2, GRB>(leds1, 100);
    FastLED.addLeds<WS2812, 3, GRB>(leds2, 100);
}

void addMoreStrips() {
    // Add more strips later (auto-regroups on next show())
    FastLED.addLeds<WS2812, 4, GRB>(leds3, 100);
    FastLED.addLeds<WS2812, 5, GRB>(leds4, 100);
    // Next FastLED.show() will detect 4-pin group
}
```

**Note:** Pin grouping is re-evaluated when the strip configuration changes.

### RGBW Support

The RP2040/RP2350 automatic parallel driver fully supports RGBW (4-channel) LED strips like SK6812.

#### Basic RGBW Usage

```cpp
#define FASTLED_RP2040_CLOCKLESS_PIO_AUTO 1
#include <FastLED.h>

#define NUM_LEDS 100
CRGB leds[NUM_LEDS];

void setup() {
    // Add RGBW strip (SK6812 or similar)
    FastLED.addLeds<WS2812, 2, GRB>(leds, NUM_LEDS).setRgbw(RgbwDefault());
}

void loop() {
    // Set colors normally - white channel calculated automatically
    fill_solid(leds, NUM_LEDS, CRGB::White);
    FastLED.show();
}
```

#### Parallel RGBW Strips

Multiple RGBW strips work with automatic parallel grouping:

```cpp
CRGB leds1[100], leds2[100], leds3[100], leds4[100];

void setup() {
    // All 4 strips will output in parallel (GPIO 2-5)
    FastLED.addLeds<WS2812, 2, GRB>(leds1, 100).setRgbw(RgbwDefault());
    FastLED.addLeds<WS2812, 3, GRB>(leds2, 100).setRgbw(RgbwDefault());
    FastLED.addLeds<WS2812, 4, GRB>(leds3, 100).setRgbw(RgbwDefault());
    FastLED.addLeds<WS2812, 5, GRB>(leds4, 100).setRgbw(RgbwDefault());
}
```

#### Mixed RGB and RGBW Strips

You can mix RGB and RGBW strips in the same parallel group:

```cpp
CRGB rgb_leds[100];
CRGB rgbw_leds[100];

void setup() {
    // GPIO 2: RGB strip (WS2812)
    FastLED.addLeds<WS2812, 2, GRB>(rgb_leds, 100);

    // GPIO 3: RGBW strip (SK6812)
    FastLED.addLeds<WS2812, 3, GRB>(rgbw_leds, 100).setRgbw(RgbwDefault());

    // These will be grouped for parallel output
}
```

**Important:** When mixing RGB and RGBW strips in a parallel group:
- The entire group operates in RGBW mode (4 bytes per LED)
- RGB strips have their W channel set to 0 (no visible effect)
- This simplifies hardware configuration (single PIO program per group)
- Slight memory overhead for RGB strips, but negligible in practice

#### RGBW Performance

**Frame time impact:**
- RGBW: ~33% more data to transfer (4 bytes vs 3)
- PIO timing: ~1.25 µs per byte (WS2812 protocol)
- 100 RGBW LEDs: ~500 µs vs ~375 µs for RGB

### Debug Output

Enable FastLED debug output to see grouping decisions:

```cpp
#define FASTLED_DEBUG_LEVEL 1
#define FASTLED_RP2040_CLOCKLESS_PIO_AUTO 1
#include <FastLED.h>
```

## Technical Details

### Data Flow

1. **User calls FastLED.show()**
2. Each `addLeds<>()` controller is a `fl::SlimBridgeController` that enqueues a channel onto `ChannelEngineRpPio`
3. The engine batches consecutive pins with matching length and timing into multi-lane state machines
4. Output starts via PIO + DMA, and independent batches run concurrently

### Architecture Diagram

```
┌──────────────────────────────────────────────────────────────────┐
│ User Code: FastLED.addLeds<WS2812, PIN>()                       │
└──────────────────┬───────────────────────────────────────────────┘
                   │
                   ▼
┌──────────────────────────────────────────────────────────────────┐
│ fl::SlimBridgeController (one per addLeds() call)               │
└──────────────────┬───────────────────────────────────────────────┘
                   │
                   ▼
┌──────────────────────────────────────────────────────────────────┐
│ ChannelEngineRpPio (PIO0, shared with Channel API)              │
│ - Batches consecutive pins w/ matching length + timing          │
│ - One multi-lane SM + DMA per batch; batches run concurrently   │
└─────────────────────────────────────────────────────────────────┘
```

## Examples

See `examples/SpecialDrivers/RP/Parallel_IO/Parallel_IO.ino` for a complete working example.

## License

This driver is part of FastLED and is licensed under the MIT License.

## Contributing

Found a bug? Have a suggestion? Please open an issue or pull request on GitHub:
https://github.com/FastLED/FastLED

## Version History

- **v1.0.0** (2025-01): Initial release
  - Automatic consecutive pin detection
  - 2/4/8-lane parallel output
  - Integration with standard FastLED API
  - RectangularDrawBuffer for multi-strip management
- **#4635**: Auto path moved onto the slim bridge / `ChannelEngineRpPio`; `RP2040ParallelGroup`, `RectangularDrawBuffer` and `parallel_transpose.h` removed
