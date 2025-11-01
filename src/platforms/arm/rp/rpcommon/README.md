# RP2xxx Common Code

This directory contains shared code for all Raspberry Pi RP2xxx platforms (RP2040, RP2350, and future variants). All code in this directory is platform-independent and works across the entire RP2xxx family.

## Files Overview

### PIO-Based Clockless LED Driver

#### `clockless_rp_pio.h`
**Purpose**: Core PIO-based implementation for clockless LED protocols (WS2812, WS2811, SK6812, etc.)

**Key Features**:
- Hardware-timed waveforms using PIO state machines
- DMA-based data transfer for efficient CPU usage
- Automatic PIO instance selection (2 PIOs on RP2040, 3 on RP2350)
- Fallback to Cortex-M0 bit-banging if PIO unavailable
- Supports all common clockless LED protocols with configurable timing

**Platform Adaptation**:
```cpp
// Automatically adapts to platform PIO count
#if defined(PICO_RP2040)
    const PIO pios[NUM_PIOS] = { pio0, pio1 };        // 2 PIOs
#elif defined(PICO_RP2350)
    const PIO pios[NUM_PIOS] = { pio0, pio1, pio2 };  // 3 PIOs
#endif
```

**Usage**: Platform-specific wrappers include this file:
- `rp2040/clockless_arm_rp2040.h` includes `../rpcommon/clockless_rp_pio.h`
- `rp2350/clockless_arm_rp2350.h` includes `../rpcommon/clockless_rp_pio.h`

---

### PIO Programming Infrastructure

#### `pio_asm.h`
**Purpose**: Macros for generating PIO assembly instructions programmatically

**Key Macros**:
```cpp
pio_out(pins, bit_count)      // Output to pins
pio_set(destination, value)   // Set scratch/pins/pindirs
pio_nop()                     // No operation (used for timing)
pio_jmp(condition, label)     // Conditional jump
pio_wait(polarity, source)    // Wait for condition
pio_in(source, bit_count)     // Input from source
pio_push(if_full, block)      // Push to RX FIFO
pio_pull(if_empty, block)     // Pull from TX FIFO
pio_mov(destination, source)  // Move data
pio_irq(mode, num)            // Trigger/clear/wait IRQ
```

**Example Usage**:
```cpp
// Build PIO program for WS2812 LEDs
uint16_t program[] = {
    pio_out(pio_pins, 1),       // Output 1 bit to pins
    pio_jmp(x_dec, loop_start), // Decrement X, jump if not zero
    pio_set(pins, 0),           // Set pins low
    // ...
};
```

**Platform Independence**: PIO instruction encoding is identical across all RP2xxx platforms.

---

#### `pio_gen.h`
**Purpose**: Generates complete PIO programs for clockless LED protocols at runtime

**Key Function**:
```cpp
void generate_pio_program(
    uint16_t* program,           // Output: Generated PIO instructions
    int T1, int T2, int T3,      // Timing: T1=high, T2=low(1), T3=low(0) in cycles
    int* wrap_target,            // Output: Program loop start
    int* wrap                    // Output: Program loop end
);
```

**How It Works**:
1. Takes LED timing requirements (e.g., WS2812: T1=800ns, T2=450ns, T3=850ns)
2. Converts timing to PIO clock cycles based on CPU frequency
3. Generates optimal PIO instruction sequence with correct delays
4. Returns ready-to-load PIO program

**Example Timing**:
```cpp
// WS2812 timing at 125 MHz (RP2040)
T1 = 100 cycles  // 800ns high pulse (for '1' bit)
T2 = 56 cycles   // 450ns high pulse (for '0' bit)
T3 = 106 cycles  // 850ns low pulse

// Same timing at 150 MHz (RP2350) automatically scales
T1 = 120 cycles  // Still 800ns
T2 = 68 cycles   // Still 450ns
T3 = 128 cycles  // Still 850ns
```

**Platform Independence**: Timing calculations automatically adapt to F_CPU.

---

### Parallel Hardware SPI Drivers

These drivers use PIO and DMA to control multiple SPI-based LED strips (APA102, SK9822, etc.) simultaneously.

#### `spi_hw_2_rp.cpp`
**Purpose**: Dual-lane parallel SPI driver - control 2 strips simultaneously

**Pin Configuration**:
```cpp
// Strip 0: Default SPI pins
Clock: GPIO 18
Data:  GPIO 19

// Strip 1: Secondary pins
Clock: GPIO 20
Data:  GPIO 21
```

**Features**:
- 2 independent clock signals (PIO-generated)
- 2 independent data signals
- Synchronized frame updates
- DMA transfer for both strips
- Up to 10-20 MHz SPI clock

---

#### `spi_hw_4_rp.cpp`
**Purpose**: Quad-lane parallel SPI driver - control 4 strips simultaneously

**Pin Configuration**:
```cpp
// Strips 0-3 use sequential pins starting from base
Clock: GPIO 2, 4, 6, 8
Data:  GPIO 3, 5, 7, 9
```

**Resource Usage**:
- 2 PIO state machines (2 clocks + 2 data each)
- 1 DMA channel
- Works on RP2040 (2 PIOs) and RP2350 (3 PIOs)

---

#### `spi_hw_8_rp.cpp`
**Purpose**: Octal-lane parallel SPI driver - control 8 strips simultaneously

**Pin Configuration**:
```cpp
// Strips 0-7 use sequential pins
Clock: GPIO 2, 4, 6, 8, 10, 12, 14, 16
Data:  GPIO 3, 5, 7, 9, 11, 13, 15, 17
```

**Resource Usage**:
- 4 PIO state machines (uses both pio0 and pio1 on RP2040)
- 1 DMA channel
- Requires careful pin planning to avoid conflicts

**Platform Considerations**:
- RP2040: Uses both PIO instances (pio0, pio1)
- RP2350: Can use pio0+pio1, leaving pio2 available for other uses

---

### System Definitions

#### `led_sysdefs_rp_common.h`
**Purpose**: Common system definitions shared across all RP2xxx platforms

**Defines**:
```cpp
// Interrupt control
namespace fl {
inline void interrupts() { /* enable interrupts */ }
inline void noInterrupts() { /* disable interrupts */ }
}  // namespace fl
// Delay functions (if not provided by framework)
#ifndef delay
#define delay(ms) /* platform delay */
#endif

// Common RP-specific macros
#define FL_CLOCKLESS_CONTROLLER_DEFINED 1  // PIO clockless support available
```

**Usage**: Included by platform-specific `led_sysdefs_arm_rpXXXX.h` files:
```cpp
// In rp2040/led_sysdefs_arm_rp2040.h
#include "../rpcommon/led_sysdefs_rp_common.h"
#ifndef F_CPU
#define F_CPU 125000000  // RP2040: 125 MHz default
#endif

// In rp2350/led_sysdefs_arm_rp2350.h
#include "../rpcommon/led_sysdefs_rp_common.h"
#ifndef F_CPU
#define F_CPU 150000000  // RP2350: 150 MHz default
#endif
```

---

## Why Common Code?

### Benefits of Shared Implementation

1. **Single Source of Truth**: Bug fixes and improvements automatically benefit all RP platforms
2. **Easier Maintenance**: Update PIO logic once, works everywhere
3. **Future-Proof**: New RP variants (RP2040B, RP3000, etc.) get full support immediately
4. **Consistency**: Identical behavior across platforms reduces testing burden
5. **Reduced Duplication**: ~3KB of PIO code shared vs duplicated = significant savings

### Platform-Specific Adaptations

The common code uses these macros to adapt to platform differences:

```cpp
// PIO instance count (defined by pico-sdk)
NUM_PIOS  // 2 on RP2040, 3 on RP2350

// Platform detection
PICO_RP2040   // Defined for RP2040
PICO_RP2350   // Defined for RP2350

// CPU frequency (defined in platform-specific headers)
F_CPU  // 125000000 (RP2040) or 150000000 (RP2350)
```

---

## PIO Resource Allocation

### PIO Instance Usage

Each RP2xxx chip has multiple PIO instances, each containing 4 state machines:

**RP2040** (2 PIOs × 4 state machines = 8 total):
```
pio0: [sm0] [sm1] [sm2] [sm3]
pio1: [sm0] [sm1] [sm2] [sm3]
```

**RP2350** (3 PIOs × 4 state machines = 12 total):
```
pio0: [sm0] [sm1] [sm2] [sm3]
pio1: [sm0] [sm1] [sm2] [sm3]
pio2: [sm0] [sm1] [sm2] [sm3]  ← Additional PIO instance
```

### FastLED Resource Usage

**Clockless LEDs** (1 strip):
- 1 PIO state machine
- 1 DMA channel
- 1 GPIO pin (data output)

**Parallel SPI (2 strips)**:
- 1 PIO state machine (clock generation)
- 1 DMA channel
- 4 GPIO pins (2 clock + 2 data)

**Parallel SPI (4 strips)**:
- 2 PIO state machines
- 1 DMA channel
- 8 GPIO pins (4 clock + 4 data)

**Parallel SPI (8 strips)**:
- 4 PIO state machines
- 1 DMA channel
- 16 GPIO pins (8 clock + 8 data)

---

## Technical Details

### PIO Program Loading

```cpp
// 1. Generate program based on LED timing
uint16_t program[32];
int wrap_target, wrap;
generate_pio_program(program, T1, T2, T3, &wrap_target, &wrap);

// 2. Load program into PIO instance
PIO pio = pio0;  // or pio1, pio2
uint offset = pio_add_program(pio, &program_config);

// 3. Configure state machine
uint sm = pio_claim_unused_sm(pio, true);
pio_sm_config config = pio_get_default_sm_config();
sm_config_set_wrap(&config, offset + wrap_target, offset + wrap);

// 4. Start state machine
pio_sm_init(pio, sm, offset, &config);
pio_sm_set_enabled(pio, sm, true);
```

### DMA Configuration

```cpp
// Configure DMA channel for LED data transfer
int dma_chan = dma_claim_unused_channel(true);

dma_channel_config config = dma_channel_get_default_config(dma_chan);
channel_config_set_transfer_data_size(&config, DMA_SIZE_32);  // 32-bit transfers
channel_config_set_dreq(&config, pio_get_dreq(pio, sm, true)); // PIO TX FIFO
channel_config_set_read_increment(&config, true);   // Increment source
channel_config_set_write_increment(&config, false); // Fixed destination (PIO FIFO)

dma_channel_configure(
    dma_chan, &config,
    &pio->txf[sm],      // Destination: PIO TX FIFO
    led_buffer,         // Source: LED data in RAM
    num_leds * 3,       // Transfer count (RGB bytes)
    true                // Start immediately
);
```

---

## Performance Characteristics

### Clockless LED Performance (WS2812)

| LEDs | Data Size | Transfer Time | CPU Usage |
|------|-----------|---------------|-----------|
| 100  | 300 bytes | ~3.0 ms       | < 1% CPU  |
| 500  | 1500 bytes| ~15 ms        | < 1% CPU  |
| 1000 | 3000 bytes| ~30 ms        | < 1% CPU  |

**Notes**:
- Transfer happens in background via DMA
- CPU free during transfer (except initial setup)
- Timing: 30 µs per LED (WS2812 protocol limitation)

### Parallel SPI Performance (APA102)

| Strips | LEDs/Strip | Clock Speed | Total Time | CPU Usage |
|--------|------------|-------------|------------|-----------|
| 2      | 100        | 10 MHz      | ~2.4 ms    | < 1% CPU  |
| 4      | 100        | 10 MHz      | ~2.4 ms    | < 1% CPU  |
| 8      | 100        | 10 MHz      | ~2.4 ms    | < 1% CPU  |

**Notes**:
- All strips update simultaneously (true parallel)
- Limited by SPI clock speed, not LED count
- DMA handles all data transfer

---

## Debugging Tips

### PIO Program Issues

If LEDs aren't working correctly:

1. **Check timing calculations**: Verify T1, T2, T3 values match LED datasheet
2. **Monitor PIO state machine**: Use `pio_sm_get_pc()` to check if program is running
3. **Verify GPIO configuration**: Ensure pin is set as output and connected to correct PIO
4. **Check F_CPU accuracy**: Wrong F_CPU causes incorrect timing

### DMA Issues

If data isn't transferring:

1. **Check DMA channel status**: Use `dma_channel_is_busy()` to verify transfer in progress
2. **Verify buffer alignment**: Some platforms require aligned buffers
3. **Check transfer count**: Ensure count matches actual data size
4. **Monitor FIFO**: Check if PIO TX FIFO is full/empty as expected

### Common Mistakes

```cpp
// ❌ WRONG: Direct PIO register access without claiming SM
pio_sm_set_enabled(pio0, 0, true);  // May conflict with other code

// ✅ CORRECT: Claim state machine first
uint sm = pio_claim_unused_sm(pio0, true);
pio_sm_set_enabled(pio0, sm, true);

// ❌ WRONG: Incorrect F_CPU
#define F_CPU 125000000  // But actually running at 133 MHz = timing errors!

// ✅ CORRECT: Match F_CPU to actual clock speed
#define F_CPU 133000000  // Correct for 133 MHz overclock
```

---

## Adding New LED Protocols

To add support for a new clockless LED protocol (e.g., WS2813, SK6805):

1. **Determine timing requirements** from LED datasheet:
   ```
   T1 = High time for '1' bit (e.g., 580ns)
   T2 = High time for '0' bit (e.g., 220ns)
   T3 = Low time for both bits (e.g., 580ns)
   ```

2. **Calculate PIO cycles** based on F_CPU:
   ```cpp
   // At 125 MHz (8ns per cycle)
   T1_cycles = (580ns / 8ns) = 73 cycles
   T2_cycles = (220ns / 8ns) = 28 cycles
   T3_cycles = (580ns / 8ns) = 73 cycles
   ```

3. **Generate PIO program**:
   ```cpp
   uint16_t program[32];
   int wrap_target, wrap;
   generate_pio_program(program, 73, 28, 73, &wrap_target, &wrap);
   ```

4. **No changes to common code needed** - timing values are the only difference!

---

## Contributing

When modifying common code:

1. **Test on multiple platforms**: Changes must work on both RP2040 and RP2350
2. **Avoid platform-specific logic**: Use `NUM_PIOS` and `F_CPU` for adaptation
3. **Document timing assumptions**: Comment PIO cycle counts and timing requirements
4. **Verify resource usage**: Ensure PIO/DMA/pin usage is clearly documented

---

## References

- [RP2040 PIO Datasheet Section](https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf#pio)
- [RP2350 PIO Enhancements](https://datasheets.raspberrypi.com/rp2350/rp2350-datasheet.pdf#pio)
- [Pico SDK PIO Examples](https://github.com/raspberrypi/pico-examples/tree/master/pio)
- [WS2812 Timing Specification](https://cdn-shop.adafruit.com/datasheets/WS2812.pdf)
- [APA102 Timing Specification](https://cdn-shop.adafruit.com/product-files/2343/APA102C.pdf)

---

**Last Updated**: October 2024
**Maintainer**: FastLED Project
**Status**: Production Ready
