# FastLED Platform: RP2040

Raspberry Pi Pico (RP2040) support.

## Files (quick pass)
- `fastled_arm_rp2040.h`: Aggregator; includes pin and clockless.
- `fastpin_arm_rp2040.h`: Pin helpers.
- `clockless_arm_rp2040.h`: Clockless driver using PIO.
- `pio_asm.h`, `pio_gen.h`: PIO assembly and program generator for T1/T2/T3‑tuned clockless output.
- `led_sysdefs_arm_rp2040.h`: System defines for RP2040.
- `spi_hw_2_rp2040.cpp`: Dual-lane (2-bit) parallel SPI driver using PIO + DMA.
- `spi_hw_4_rp2040.cpp`: Quad-lane (4-bit) parallel SPI driver using PIO + DMA.
- `spi_hw_8_rp2040.cpp`: Octal-lane (8-bit) parallel SPI driver using PIO + DMA.

Notes:
- Uses PIO program assembled at runtime; ensure T1/T2/T3 match LED timing.
 - `clockless_arm_rp2040.h` configures wrap targets and delays via `pio_gen.h`; changes to timing require regenerating the program.

## Hardware SPI Support

The RP2040/RP2350 platforms support parallel SPI output for controlling multiple LED strips simultaneously using the Programmable I/O (PIO) subsystem combined with DMA transfers.

### Supported Configurations

- **Dual-lane (2-bit)**: Control 2 LED strips in parallel
- **Quad-lane (4-bit)**: Control 4 LED strips in parallel
- **Octal-lane (8-bit)**: Control 8 LED strips in parallel

### Implementation Details

Unlike traditional hardware SPI controllers that only support single-lane operation, the RP2040/RP2350 implementations use **PIO state machines** to achieve multi-lane parallel output:

- **PIO-based**: Each SPI controller uses one PIO state machine to drive multiple data pins synchronously
- **DMA transfers**: Non-blocking asynchronous data transfers using dedicated DMA channels
- **Clock generation**: Configurable SPI clock frequency (up to 25 MHz)
- **Two controllers**: SPI0 and SPI1 (two independent PIO state machines available)

### Pin Requirements

#### Dual-lane (2-bit) SPI
- 1 clock pin (SCK)
- 2 consecutive data pins (D0, D1)

#### Quad-lane (4-bit) SPI
- 1 clock pin (SCK)
- 4 consecutive data pins (D0, D1, D2, D3)
- Auto-detection: Can operate in 1-lane, 2-lane, or 4-lane mode based on active pins

#### Octal-lane (8-bit) SPI
- 1 clock pin (SCK)
- 8 consecutive data pins (D0-D7)
- **Important**: All 8 data pins must be consecutive GPIO numbers

### Resource Usage

Each active SPI controller allocates:
- **1 PIO state machine** (from available pool: 2 PIOs × 4 SMs on RP2040, 3 PIOs × 4 SMs on RP2350)
- **1 DMA channel** (from 12 available channels)
- **PIO program memory**: ~32 instructions per controller

### Usage Example

```cpp
#include <FastLED.h>

// Configure dual-lane SPI for 2 LED strips
SpiHw2::Config config;
config.bus_num = 0;           // Use SPI0 (PIO state machine 0)
config.clock_speed_hz = 4000000;  // 4 MHz
config.sck_pin = 2;           // Clock pin
config.data_pins[0] = 3;      // First strip data pin
config.data_pins[1] = 4;      // Second strip data pin

auto controllers = SpiHw2::getAll();
if (!controllers.empty()) {
    controllers[0]->begin(config);
    // Ready to transmit data
}
```

### Performance Characteristics

- **Maximum clock speed**: ~25 MHz (PIO-limited)
- **DMA overhead**: <1 µs transfer initiation
- **Async operation**: Non-blocking transmitAsync() + waitComplete()
- **Memory overhead**: Minimal (no internal buffering required for DMA-safe buffers)

### Platform Support

- **RP2040** (Raspberry Pi Pico): Fully supported
- **RP2350** (Raspberry Pi Pico 2): Fully supported
- Platform detection uses `PICO_RP2040` and `PICO_RP2350` macros

### Limitations

- Data pins must be consecutive GPIO numbers for multi-lane modes
- Maximum 2 independent controllers (limited by PIO state machine availability)
- PIO program space is shared with other PIO-based features (e.g., clockless LED drivers)

## Optional feature defines

- **`FASTLED_ALLOW_INTERRUPTS`**: Allow ISRs during show. Default `1`.
- **`FASTLED_ACCURATE_CLOCK`**: Enabled when interrupts are allowed to maintain timing math accuracy.
- **`FASTLED_USE_PROGMEM`**: Default `0` (flat memory model).
- **Clockless driver selection/tuning**
  - **`FASTLED_RP2040_CLOCKLESS_PIO`**: Use PIO engine for clockless. Default `1`.
  - **`FASTLED_RP2040_CLOCKLESS_IRQ_SHARED`**: Share IRQ usage between PIO and other subsystems. Default `1`.
  - **`FASTLED_RP2040_CLOCKLESS_M0_FALLBACK`**: Fallback to a Cortex‑M0 timing loop if PIO is disabled/unavailable. Default `0`.

Define these before including `FastLED.h` in your sketch.
