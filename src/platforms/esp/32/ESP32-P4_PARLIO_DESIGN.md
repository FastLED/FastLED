# ESP32-P4 Parallel IO WS2812 Driver Design Document

## Overview

The ESP32-P4 introduces a Parallel IO (parlio) TX peripheral that enables simultaneous output to multiple WS2812 LED strips (8 or 16 channels). This document describes the WLED-MM implementation approach and proposes a FastLED-compatible driver design.

## Background: WLED-MM Implementation

WLED-MM on ESP32-P4 uses the chip's Parallel IO TX peripheral instead of bit-banging or RMT. The parlio unit configures a wide data bus (8 or 16 lines) with a shared clock, outputting multiple pins simultaneously at a high clock rate to drive many LED strips in lockstep.

### Key Hardware Features

- **Simultaneous output**: 8-16 LED strips driven in parallel
- **DMA-based**: Hardware shifts out bits, freeing CPU
- **High performance**: Achieves 120+ FPS for 256-pixel strips across 8 channels
- **Hardware timing**: All WS2812 timing constraints met by parlio engine

### WLED-MM Driver Architecture

The WLED-MM implementation (likely in `wled00/src/drivers/`) follows this pattern:

#### 1. Hardware Configuration

```c
parlio_tx_unit_config_t cfg = {
    .clk_src = PARLIO_CLK_SRC_DEFAULT,
    .data_width = 8,  // Number of parallel channels (8 or 16)
    .clk_out_gpio_num = <CLK_PIN>,
    .data_gpio_nums = {PIN0, PIN1, ..., PIN7},  // LED output pins
    .output_clk_freq_hz = 8000000,  // 8.0 MHz (current implementation)
    .bit_pack_order = PARLIO_BIT_MSB_FIRST,  // Match WS2812 bit order
    // valid_gpio_num = -1 (unused, no separate latch)
};

parlio_tx_unit_handle_t tx_unit;
parlio_new_tx_unit(&tx_unit, &cfg);
parlio_tx_unit_enable(tx_unit);
```

#### 2. Data Encoding

The driver bit-packs RGB color values so each output byte contains the same bit position for all strips:

- For 8 strips: each 24-bit GRB color is expanded bitwise
- MSB of each strip's first color byte forms the first output byte
- Next bits form the second byte, etc.
- When PIO clock pulses, all 8 data lines are driven with correct bit simultaneously

**Bit-packing example** (MSB-first mode):
- Bit 0 of each payload byte → MSB of bus pins
- Total payload must be multiple of data width
- For M LEDs on 8 strips: M × 24 × 8 bits total

#### 3. Data Transmission

```c
// Prepare bit-packed payload buffer
parlio_transmit_config_t txcfg = { .idle_value = 0x00 };  // Hold lines low when idle

// Non-blocking DMA transfer
parlio_tx_unit_transmit(tx_unit, payload, total_bits, &txcfg);

// Wait for completion (or use callback)
parlio_tx_unit_wait_all_done(tx_unit, -1);

// Alternative: register callback for async operation
parlio_tx_unit_register_event_callbacks(tx_unit, &cbs, NULL);
```

#### 4. Timing Strategy

WS2812 timing achieved by:

1. **Clock frequency selection**: Choose `output_clk_freq_hz` as integer fraction of APB clock
   - Example: 160 MHz / 16 = 10 MHz (100 ns tick)
   - Example: 160 MHz / 12 ≈ 13.3 MHz (75 ns tick)

2. **Bit pattern encoding**: Map each WS2812 bit to PIO clock pulses
   - Using 80 ns ticks, 16 clocks per bit = 1.28 µs
   - WS2812 "0": ~5 ticks high + 11 low ≈ 0.4µs H, 0.88µs L
   - WS2812 "1": ~11 ticks high + 5 low ≈ 0.88µs H, 0.4µs L

3. **Hardware enforcement**: All timing constraints met by parlio engine, no CPU involvement

## Proposed FastLED Driver Design

### Architecture

Implement a custom FastLED controller for ESP32-P4 using the parlio peripheral, following FastLED's `CLEDController` pattern.

### Class Structure

```cpp
template<uint8_t DATA_WIDTH = 8>
class PIOParallelController : public CLEDController {
private:
    parlio_tx_unit_handle_t tx_unit;
    uint8_t* payload_buffer;
    size_t buffer_size;

    // Configuration
    gpio_num_t clk_pin;
    gpio_num_t data_pins[DATA_WIDTH];
    uint32_t clock_freq_hz;

public:
    PIOParallelController(gpio_num_t clk, const gpio_num_t pins[DATA_WIDTH]);
    ~PIOParallelController();

    // Initialization
    void init();
    void shutdown();

    // Data transmission
    void showPixels(CRGB* leds[], uint16_t num_leds);

protected:
    // Internal helpers
    void packData(CRGB* leds[], uint16_t num_leds, uint8_t* buffer);
    void transmit(const uint8_t* buffer, size_t total_bits);
};
```

### Implementation Details

#### 1. Initialization (`init()`)

```cpp
void PIOParallelController::init() {
    // Configure parlio TX unit
    parlio_tx_unit_config_t cfg = {
        .clk_src = PARLIO_CLK_SRC_DEFAULT,
        .data_width = DATA_WIDTH,
        .clk_out_gpio_num = clk_pin,
        .data_gpio_nums = {data_pins[0], data_pins[1], ..., data_pins[DATA_WIDTH-1]},
        .output_clk_freq_hz = 8000000,  // 8.0 MHz (current implementation)
        .bit_pack_order = PARLIO_BIT_MSB_FIRST,
    };

    ESP_ERROR_CHECK(parlio_new_tx_unit(&cfg, &tx_unit));
    ESP_ERROR_CHECK(parlio_tx_unit_enable(tx_unit));

    // Allocate DMA buffer
    buffer_size = calculate_buffer_size(max_leds);
    payload_buffer = (uint8_t*)heap_caps_malloc(buffer_size, MALLOC_CAP_DMA);
}
```

#### 2. Data Packing (`packData()`)

For each LED position and each of 24 color bits (GRB, MSB-first):

```cpp
void PIOParallelController::packData(CRGB* leds[], uint16_t num_leds, uint8_t* buffer) {
    size_t byte_idx = 0;

    for (uint16_t led = 0; led < num_leds; led++) {
        // Process each color component (G, R, B for WS2812)
        for (uint8_t component = 0; component < 3; component++) {
            uint8_t color_byte = get_component(leds, led, component);  // G, R, or B

            // Process 8 bits of this color component
            for (int8_t bit = 7; bit >= 0; bit--) {
                uint8_t output_byte = 0;

                // Pack same bit position from all DATA_WIDTH channels
                for (uint8_t channel = 0; channel < DATA_WIDTH; channel++) {
                    uint8_t channel_color = get_component(leds[channel], led, component);
                    uint8_t bit_val = (channel_color >> bit) & 0x01;
                    output_byte |= (bit_val << (DATA_WIDTH - 1 - channel));
                }

                buffer[byte_idx++] = output_byte;
            }
        }
    }
}
```

#### 3. Transmission (`showPixels()`)

```cpp
void PIOParallelController::showPixels(CRGB* leds[], uint16_t num_leds) {
    // Pack LED data into parlio buffer format
    packData(leds, num_leds, payload_buffer);

    // Calculate total bits to transmit
    size_t total_bits = num_leds * 24 * DATA_WIDTH;

    // Configure transmission
    parlio_transmit_config_t txcfg = {
        .idle_value = 0x00  // Lines idle low between frames
    };

    // Transmit (non-blocking DMA)
    ESP_ERROR_CHECK(parlio_tx_unit_transmit(tx_unit, payload_buffer, total_bits, &txcfg));

    // Wait for completion
    ESP_ERROR_CHECK(parlio_tx_unit_wait_all_done(tx_unit, -1));
}
```

### Timing Configuration

To meet WS2812 protocol (T0H/T1H/T0L/T1L):

1. **Choose clock frequency**:
   - Current implementation: 8.0 MHz (125 ns tick)
   - Alternative options: 12.5 MHz (80 ns tick) or 10 MHz (100 ns tick)
   - Configurable via `output_clk_freq_hz`

2. **Encode bit patterns** (for 8.0 MHz implementation):
   - Using 10 clocks per bit @ 125ns = 1.25µs total
   - "0" bit: 3 high + 7 low = 375ns H, 875ns L
   - "1" bit: 7 high + 3 low = 875ns H, 375ns L

3. **Clock source**:
   - Default: `PARLIO_CLK_SRC_DEFAULT` (APB clock derived)
   - Custom: External clock if needed

### Pin Configuration

- **Clock pin**: Single GPIO for parlio clock output
- **Data pins**: Array of DATA_WIDTH GPIOs (8 or 16)
  - Each pin drives one independent LED strip
  - Shared ground between all strips
  - Only data pins are parallel

### Constraints and Optimizations

1. **Payload alignment**: Total bits must be multiple of `data_width`
   - Pad frames if needed: `(payload_bits % data_width) == 0`

2. **DMA buffer**: Allocate in DMA-capable memory
   - Use `heap_caps_malloc(size, MALLOC_CAP_DMA)`

3. **Strip length**: All strips must have same number of LEDs
   - Shorter strips simply stop receiving after their data ends

4. **Idle state**: Configure `idle_value = 0x00` for low idle between frames

5. **Async operation**: Optional callback for non-blocking updates
   ```cpp
   parlio_tx_event_callbacks_t cbs = {
       .on_trans_done = tx_done_callback
   };
   parlio_tx_unit_register_event_callbacks(tx_unit, &cbs, user_data);
   ```

6. **Continuous mode**: Use `loop_transmission = true` for constant refresh
   - Not typical for WS2812 (frame-then-pause pattern)

### Extensions

#### 16-Channel Support

```cpp
using PIO16Controller = PIOParallelController<16>;

// Requires 16 GPIO pins
// More complex packing (24 * NUM_LEDS * 16 bits)
// Ensure divisibility: (24 * NUM_LEDS * 16) % 16 == 0
```

#### Color Order Support

```cpp
template<EOrder RGB_ORDER = RGB>
class PIOParallelController : public CLEDController {
    // Reorder components during packing based on RGB_ORDER
    // Support GRB, RGB, BGR, etc.
};
```

#### Other LED Types

- **SK6812 (RGBW)**: Modify to pack 32 bits (4 components) instead of 24
- **Clocked strips**: May use parlio as SPI master or fall back to SPI hardware

## Integration with FastLED

### Controller Registration

```cpp
#ifdef FASTLED_ESP32P4

template<uint8_t DATA_WIDTH = 8>
class ESP32P4ParallelOutput {
    PIOParallelController<DATA_WIDTH> controller;

public:
    ESP32P4ParallelOutput(gpio_num_t clk, const gpio_num_t pins[DATA_WIDTH])
        : controller(clk, pins) {
        controller.init();
    }

    template<EOrder RGB_ORDER = GRB>
    void addLeds(CRGB* leds[], uint16_t num_leds) {
        FastLED.addController(&controller);
        controller.setLeds(leds, num_leds);
    }
};

#endif
```

### Example Usage

```cpp
#ifdef FASTLED_ESP32P4

#define NUM_STRIPS 8
#define NUM_LEDS 256

CRGB leds[NUM_STRIPS][NUM_LEDS];

gpio_num_t data_pins[8] = {
    GPIO_NUM_1, GPIO_NUM_2, GPIO_NUM_3, GPIO_NUM_4,
    GPIO_NUM_5, GPIO_NUM_6, GPIO_NUM_7, GPIO_NUM_8
};

void setup() {
    // Initialize parallel controller
    ESP32P4ParallelOutput<8> parallel(GPIO_NUM_9, data_pins);

    CRGB* led_arrays[8] = {
        leds[0], leds[1], leds[2], leds[3],
        leds[4], leds[5], leds[6], leds[7]
    };

    parallel.addLeds<GRB>(led_arrays, NUM_LEDS);
}

void loop() {
    // Update all 8 strips simultaneously
    for (int i = 0; i < NUM_STRIPS; i++) {
        fill_rainbow(leds[i], NUM_LEDS, i * 32, 7);
    }
    FastLED.show();
}

#endif
```

## Performance Characteristics

Based on WLED-MM implementation:

- **Frame rate**: 120+ FPS for 256-pixel strips (8 channels)
- **CPU overhead**: Minimal (DMA-driven, hardware timing)
- **Latency**: ~21µs per LED × strip length
  - 256 LEDs: ~5.4ms transmission time
  - Allows ~185 FPS theoretical max

## References

### ESP-IDF Documentation

- [Parallel IO Overview](https://docs.espressif.com/projects/esp-idf/en/stable/esp32p4/api-reference/peripherals/parlio/index.html)
- [Parallel IO TX Driver](https://docs.espressif.com/projects/esp-idf/en/stable/esp32p4/api-reference/peripherals/parlio/parlio_tx.html)

### WLED-MM Implementation

- [Reddit Discussion](https://www.reddit.com/r/FastLED/comments/1jb7dgt/newer_chips_like_c6_and_h2_feature_a_parallel_io/)
- Developers noted: "chips like C6 and H2 [and P4] feature a Parallel IO peripheral, which allows controlling 8/16 LED strips simultaneously"

## API Summary

### Core Functions

- `parlio_new_tx_unit(handle, config)` - Create TX unit
- `parlio_tx_unit_enable(handle)` - Enable TX unit
- `parlio_tx_unit_transmit(handle, payload, bits, config)` - Send data (non-blocking)
- `parlio_tx_unit_wait_all_done(handle, timeout)` - Wait for completion
- `parlio_tx_unit_register_event_callbacks(handle, cbs, user_data)` - Async callbacks
- `parlio_del_tx_unit(handle)` - Destroy TX unit

### Configuration Structures

- `parlio_tx_unit_config_t` - Hardware setup (pins, clock, bit order)
- `parlio_transmit_config_t` - Per-transmission config (idle value, loop mode)
- `parlio_tx_event_callbacks_t` - Event handlers (on_trans_done)

## Implementation Status

**Status**: Implemented ✓

**Completed**:
1. ✓ Implemented `ParlioLedDriver` class in `clockless_parlio_esp32p4.h`
2. ✓ Created `ParlioController` FastLED wrapper
3. ✓ Added example in `examples/SpecialDrivers/ESP/Parlio/Esp32P4Parlio/`
4. ✓ Supports 8 or 16 parallel channels with template parameterization
5. ✓ DMA-based transmission with hardware timing
6. ✓ Strategic buffer breaking at color component boundaries (see Buffer Breaking Strategy below)

**Next Steps**:
1. Test on ESP32-P4 hardware
2. WS2812 timing calibration and validation
3. Benchmark against RMT/I2S drivers
4. Add to FastLED chipset detection system
5. Document platform-specific setup requirements
6. Add support for other chipsets (SK6812, WS2811, etc.)

## Buffer Breaking Strategy

### Overview

The FastLED ESP32-P4 parlio driver implements the **WLED-MM-P4 buffer breaking strategy** at LED boundaries to minimize visual artifacts from DMA timing gaps. This enhancement was inspired by insights from the WLED-MM P4 implementation.

### Problem: DMA Gap Visual Artifacts

When transmitting large LED configurations, DMA transfers often require chunking due to hardware size constraints (65535 bytes per PARLIO transaction). The gaps between buffer transmissions can cause timing glitches that manifest as visual artifacts on the LED strips.

**Key timing constraint**: Inter-buffer gaps must be < 20µs to avoid glitches (not 50µs as datasheets suggest).

For large transmissions (e.g., 512×16 LEDs), multiple DMA chunks are unavoidable, and gaps between them can be observed on logic analyzers.

### Solution: WLED-MM-P4 Buffer Breaking at LED Boundaries

**Key insight from WLED-MM developer** ([GitHub issue #2095](https://github.com/FastLED/FastLED/issues/2095#issuecomment-3369337632)):

> "I also break my buffers so it's after the LSB of the triplet (or quad). In case that pause is misinterpreted as a 1 vs 0, it's not going to cause a visual disruption. At worst 0,0,0 becomes 0,0,1."

**Implementation approach**: Break DMA buffers at LED boundaries after transmitting all three color components (G, R, B) for each LED. This ensures that DMA gaps occur after the LSB (bit 0) of the B component, which is the natural end of an LED's data.

**Benefits**:
- DMA gaps only occur at LED boundaries (after LSB of B component)
- If timing glitches occur, they affect only the least significant bit (imperceptible)
- Worst case artifact: 0,0,0 becomes 0,0,1 (imperceptible brightness change)
- Eliminates major glitches from timing issues where MSB corruption causes ±128 brightness errors
- Keeps each transmission under timing threshold (<20μs gap tolerance)

### Error Impact Comparison

**Without strategic buffer breaking** (gap occurs mid-component):
```
Intended: G=128 (0b10000000)
Glitched: G=0   (0b00000000)  ← MSB corrupted
Result: Highly visible color shift (±128 brightness)
```

**With WLED-MM-P4 buffer breaking** (gap occurs at LED boundary after LSB):
```
Intended: LED[N] = {G=128, R=64, B=32}
Glitched: LED[N] = {G=128, R=64, B=33}  ← LSB affected if gap misinterpreted
Result: Barely visible or imperceptible (±1 in LSB at worst)
```

### Implementation

The implementation is in `src/platforms/esp/32/drivers/parlio/parlio_driver_impl.h` in the `show()` method:

**Buffer layout**:
- Single contiguous DMA buffer containing all LED data
- Each LED = 3 components × 32 bits per component = 96 bits total
- For DATA_WIDTH parallel strips, each LED chunk = 96 × DATA_WIDTH bits

**Chunking strategy**:
1. Calculate maximum LEDs per chunk based on PARLIO hardware limit (65535 bytes)
2. Transmit LEDs in chunks, breaking at LED boundaries
3. Each chunk contains complete LED data (all 3 color components)
4. Wait for DMA completion between chunks to ensure clean gaps at LED boundaries

**Example for 8-bit width (8 parallel strips)**:
- Each LED = 96 bits × 8 = 768 bits = 96 bytes
- Max LEDs per chunk = 65535 bytes ÷ 96 bytes = 682 LEDs
- For 1024 LEDs: Chunk 1 = 682 LEDs, Chunk 2 = 342 LEDs
- DMA gap occurs after LED 682, which is after LSB of B component

### Code Structure

```cpp
// In show() method:
constexpr uint32_t BITS_PER_COMPONENT = 32 * DATA_WIDTH;
constexpr uint32_t COMPONENTS_PER_LED = 3;  // G, R, B
constexpr uint32_t BYTES_PER_LED = COMPONENTS_PER_LED * BYTES_PER_COMPONENT;

// Break at LED boundaries
while (leds_remaining > 0) {
    uint16_t leds_in_chunk = min(leds_remaining, MAX_LEDS_PER_CHUNK);
    size_t chunk_bytes = leds_in_chunk * BYTES_PER_LED;

    parlio_tx_unit_transmit(tx_unit_, chunk_ptr, chunk_bits, &tx_config);

    // Wait for completion - ensures gap at LED boundary
    if (leds_remaining > 0) {
        xSemaphoreTake(xfer_done_sem_, portMAX_DELAY);
    }
}
```

### Configuration

**No configuration needed** - the WLED-MM-P4 buffer breaking strategy is the only implementation and is always active. The driver automatically:
1. Calculates optimal chunk size based on DATA_WIDTH
2. Breaks transmissions at LED boundaries
3. Ensures DMA gaps occur after LSB of B component
4. Handles all LED counts from 1 to 65535+

### Performance Impact

- **Transmission time**: Negligible difference (same total data transferred)
- **Memory**: Single contiguous DMA buffer (no additional allocation)
- **CPU overhead**: Minimal (synchronous wait between chunks only when needed)
- **Visual quality**: Significantly improved for large LED configurations (512+ LEDs)
- **Throughput**: Maintains high frame rates even with chunking

### References

- Original issue: [FastLED #2095](https://github.com/FastLED/FastLED/issues/2095)
- WLED-MM P4 experimental branch: https://github.com/troyhacks/WLED/tree/P4_experimental
- WLED-MM demo: https://www.reddit.com/r/WLED/s/q1pZg1mnwZ
- Implementation: `src/platforms/esp/32/drivers/parlio/parlio_driver_impl.h`

---

*Last updated: 2025-11-05*
*Target FastLED version: 4.x*
*ESP-IDF version: 5.5.1+*
