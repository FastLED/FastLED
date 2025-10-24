# FastLED Platform: RP2350

Raspberry Pi Pico 2 (RP2350) support.

## Files (quick pass)
- `fastled_arm_rp2350.h`: Aggregator; includes pin and clockless.
- `fastpin_arm_rp2350.h`: Pin helpers (48 GPIO pins: 0-47).
- `clockless_arm_rp2350.h`: Clockless driver using PIO (wrapper for common implementation).
- `led_sysdefs_arm_rp2350.h`: System defines for RP2350 (150 MHz default clock).

### Shared RP Platform Files (in `../rpcommon/`)
- `clockless_rp_pio.h`: Common PIO-based clockless implementation for all RP2xxx platforms.
- `pio_asm.h`, `pio_gen.h`: PIO assembly and program generator for T1/T2/T3‑tuned clockless output.
- `spi_hw_2_rp.cpp`: Dual-lane (2-bit) parallel SPI driver using PIO + DMA.
- `spi_hw_4_rp.cpp`: Quad-lane (4-bit) parallel SPI driver using PIO + DMA.
- `spi_hw_8_rp.cpp`: Octal-lane (8-bit) parallel SPI driver using PIO + DMA.
- `led_sysdefs_rp_common.h`: Common system defines for all RP2xxx platforms.

Notes:
- Uses PIO program assembled at runtime; ensure T1/T2/T3 match LED timing.
- `clockless_rp_pio.h` configures wrap targets and delays via `pio_gen.h`; changes to timing require regenerating the program.

## RP2350 Platform Differences

### Key Features
| Feature | RP2350 | RP2040 |
|---------|--------|--------|
| **GPIO Pins** | Up to 48 pins (0-47 on RP2350B) | 30 pins (0-29) |
| **PIO Instances** | 3 (pio0, pio1, pio2) | 2 (pio0, pio1) |
| **CPU Cores** | Dual Cortex-M33 or dual Hazard3 RISC-V | Dual Cortex-M0+ |
| **Clock Speed** | 150 MHz default | 125 MHz default |
| **Memory** | 520 KB SRAM | 264 KB SRAM |

### Extended GPIO Support
The RP2350B variant supports up to 48 GPIO pins (0-47), compared to 30 pins (0-29) on RP2040. All 48 pins are defined in `fastpin_arm_rp2350.h`.

**Note**: The RP2350A variant only exposes 30 GPIO pins like RP2040, but the platform code supports all 48 for compatibility with RP2350B.

### Enhanced PIO Subsystem
The RP2350 includes a third PIO instance (pio2), providing:
- **12 total PIO state machines** (3 PIOs × 4 SMs each)
- More resources for parallel SPI and clockless LED drivers
- Better multi-strip performance

## Hardware SPI Support

The RP2350 platform supports parallel SPI output for controlling multiple LED strips simultaneously using the Programmable I/O (PIO) subsystem combined with DMA transfers.

### Supported Configurations

- **Dual-lane (2-bit)**: Control 2 LED strips in parallel
- **Quad-lane (4-bit)**: Control 4 LED strips in parallel
- **Octal-lane (8-bit)**: Control 8 LED strips in parallel

### Implementation Details

The RP2350 implementations use **PIO state machines** to achieve multi-lane parallel output:

- **PIO-based**: Each SPI controller uses one PIO state machine to drive multiple data pins synchronously
- **DMA transfers**: Non-blocking asynchronous data transfers using dedicated DMA channels
- **Clock generation**: Configurable SPI clock frequency (up to 25 MHz)
- **Three PIOs**: With 3 PIO instances available, RP2350 can support more concurrent controllers

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
- **1 PIO state machine** (from available pool: 3 PIOs × 4 SMs = 12 total on RP2350)
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
- **Async operation**: Non-blocking transmit() + waitComplete()
- **Memory overhead**: Minimal (no internal buffering required for DMA-safe buffers)
- **Enhanced performance**: 3rd PIO instance allows more concurrent operations

### Platform Support

- **RP2350** (Raspberry Pi Pico 2): Fully supported
- Platform detection uses `PICO_RP2350` macro

### Limitations

- Data pins must be consecutive GPIO numbers for multi-lane modes
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

## Clock Speed Configuration

The RP2350 defaults to 150 MHz system clock. You can override this by:

1. **Using the VARIANT_MCK macro** (if provided by your board definition):
   ```cpp
   // Automatically uses VARIANT_MCK if defined
   #include <FastLED.h>
   ```

2. **Defining F_CPU before including FastLED**:
   ```cpp
   #define F_CPU 200000000  // 200 MHz overclock
   #include <FastLED.h>
   ```

**Note**: The timing calculations for clockless LEDs depend on F_CPU being accurate. If you change the system clock at runtime using `set_sys_clock_khz()`, LED timing may be affected.
