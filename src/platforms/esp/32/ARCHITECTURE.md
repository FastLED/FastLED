# ESP32 Platform Architecture

## Overview

The ESP32 platform implementation supports 9+ ESP32 variants (ESP32 classic, S2, S3, C3, C6, H2, P4, etc.) with 6 different LED output driver types, all organized into a clean hierarchical structure.

## Directory Structure

```
src/platforms/esp/32/
├── core/                        # Core platform infrastructure
│   ├── fastled_esp32.h          # Master aggregator (driver dispatcher)
│   ├── led_sysdefs_esp32.h      # System defines
│   ├── fastpin_esp32.h          # Pin mappings (all variants)
│   ├── fastspi_esp32.h          # Hardware SPI abstraction
│   ├── clock_cycles.h           # Timing utilities
│   ├── esp_log_control.h        # Logging control
│   ├── delay.h                  # Xtensa delay
│   ├── delay_riscv.h            # RISC-V delay
│   ├── delaycycles.h            # Xtensa cycle-accurate delay
│   └── delaycycles_riscv.h      # RISC-V cycle-accurate delay
│
├── drivers/                     # LED output drivers (organized by type)
│   ├── rmt/                     # RMT (Remote Control Module)
│   │   ├── clockless_rmt_esp32.h    # RMT dispatcher
│   │   ├── rmt_4/                   # ESP-IDF 4.x implementation (8 files)
│   │   └── rmt_5/                   # ESP-IDF 5.x implementation (13 files)
│   │
│   ├── i2s/                     # I2S parallel output
│   │   ├── clockless_i2s_esp32.h     # I2S driver
│   │   ├── clockless_i2s_esp32s3.*   # S3 variant
│   │   └── i2s_esp32dev.*            # I2S helpers
│   │
│   ├── lcd_cam/                 # LCD_CAM drivers (S3/P4 only)
│   │   ├── clockless_lcd_i80_esp32.*     # I80 interface
│   │   ├── clockless_lcd_rgb_esp32.*     # RGB interface
│   │   ├── lcd_driver_*.h                # Helper files (6 files)
│   │   └── implementation_notes.md       # Implementation docs
│   │
│   ├── parlio/                  # Parallel IO (P4 only)
│   │   ├── clockless_parlio_esp32p4.*    # PARLIO driver
│   │   └── parlio_driver*.h              # Helper files
│   │
│   ├── spi/                     # Hardware SPI (clocked LEDs)
│   │   ├── spi_hw_1_esp32.cpp        # 1-lane SPI
│   │   ├── spi_hw_2_esp32.cpp        # 2-lane SPI
│   │   ├── spi_hw_4_esp32.cpp        # 4-lane SPI
│   │   ├── spi_hw_8_esp32.cpp        # 8-lane SPI (P4 only)
│   │   ├── spi_platform_esp32.cpp    # Platform abstraction
│   │   ├── spi_output_template.h     # SPI output template
│   │   └── spi_device_proxy.h        # SPI device proxy
│   │
│   └── spi_ws2812/              # Clockless SPI (WS2812 over SPI)
│       ├── clockless_spi_esp32.h     # WS2812-over-SPI driver
│       └── strip_spi.*               # SPI strip implementation
│
├── interrupts/                  # Architecture-specific ISRs
│   ├── xtensa_lx6.hpp           # ESP32 classic
│   ├── xtensa_lx7.hpp           # ESP32-S2/S3
│   ├── riscv.hpp                # ESP32-C3/C6/H2/P4
│   ├── riscv.cpp                # RISC-V implementation
│   └── INVESTIGATE.md           # Research notes
│
├── audio/                       # Audio processing subsystem
├── ota/                         # OTA update support
│
└── [Root-level files]           # Platform abstraction
    ├── interrupt.h              # Interrupt control interface
    ├── interrupt.cpp            # Interrupt control implementation
    ├── isr_esp32.cpp            # ISR API implementation (IDF 5.0+)
    └── clockless_block_esp32.h  # Experimental (not in production)
```

## Driver Dispatcher Pattern

The ESP32 platform uses a dispatcher pattern to select the appropriate LED output driver at compile time:

```
User Code (Arduino/Platform IO sketch)
    ↓
FastLED.h
    ↓
platforms/esp/32/core/fastled_esp32.h (Master Aggregator)
    ↓
    ├─→ drivers/rmt/clockless_rmt_esp32.h (Default driver)
    │       ↓
    │       ├─→ rmt_4/ (ESP-IDF 4.x - legacy)
    │       │     ├── idf4_clockless_rmt_esp32.h (ChannelBusManager-based)
    │       │     └── channel_engine_rmt4.h / channel_engine_rmt4.cpp
    │       │
    │       └─→ rmt_5/ (ESP-IDF 5.x - default, DMA-backed)
    │             ├── idf5_clockless.h (ChannelBusManager-based)
    │             ├── idf5_rmt.h / idf5_rmt.cpp
    │             ├── strip_rmt.h / strip_rmt.cpp
    │             └── rmt5_* worker pool implementation (7 files)
    │
    ├─→ drivers/i2s/clockless_i2s_esp32.h (Parallel output)
    │       ↓
    │       ├─→ clockless_i2s_esp32s3.h (ESP32-S3 variant)
    │       └─→ i2s_esp32dev.h (ESP32 classic)
    │
    ├─→ drivers/lcd_cam/clockless_lcd_i80_esp32.h (S3/P4 high-bandwidth)
    │       ↓
    │       └─→ lcd_driver_i80.h / lcd_driver_i80_impl.h
    │
    ├─→ drivers/lcd_cam/clockless_lcd_rgb_esp32.h (P4 RGB interface)
    │       ↓
    │       └─→ lcd_driver_rgb.h / lcd_driver_rgb_impl.h
    │
    ├─→ drivers/parlio/clockless_parlio_esp32p4.h (P4 only)
    │       ↓
    │       └─→ parlio_driver.h / parlio_driver_impl.h
    │
    └─→ drivers/spi_ws2812/clockless_spi_esp32.h (WS2812 over SPI)
            ↓
            └─→ strip_spi.h / strip_spi.cpp
```

## Target-Specific Code Organization

The ESP32 platform supports 9+ chip variants using inline conditional compilation rather than separate directories:

### Why No Target Directories?

**90% of code is target-agnostic** - Most driver code works identically across all ESP32 variants. Only pin definitions, interrupt handlers, and peripheral availability differ.

**Example: Pin mappings** (`core/fastpin_esp32.h`):
```cpp
#if CONFIG_IDF_TARGET_ESP32
    // ESP32 classic pin definitions
    #define NUM_DIGITAL_PINS 40
    #define FASTLED_HAS_CLOCKLESS 1
#elif CONFIG_IDF_TARGET_ESP32S3
    // ESP32-S3 pin definitions
    #define NUM_DIGITAL_PINS 49
    #define FASTLED_HAS_CLOCKLESS 1
    #define FASTLED_ESP32_S3_USB_JTAG_WORKAROUND 1  // S3-specific
#elif CONFIG_IDF_TARGET_ESP32C3
    // ESP32-C3 pin definitions (RISC-V)
    #define NUM_DIGITAL_PINS 22
    #define FASTLED_HAS_CLOCKLESS 1
    #define FASTLED_RISCV 1
// ... etc for 9+ targets
#endif
```

### Architecture-Specific Code

Architectural differences (Xtensa vs RISC-V) are handled in two ways:

1. **Timing functions**: Separate files for Xtensa and RISC-V
   - `core/delay.h` / `core/delay_riscv.h`
   - `core/delaycycles.h` / `core/delaycycles_riscv.h`

2. **Interrupt handlers**: Dedicated `interrupts/` directory
   - `interrupts/xtensa_lx6.hpp` - ESP32 classic (LX6 core)
   - `interrupts/xtensa_lx7.hpp` - ESP32-S2/S3 (LX7 core)
   - `interrupts/riscv.hpp` - ESP32-C3/C6/H2/P4 (RISC-V)

## IDF Version Branching

The RMT driver has completely separate implementations for ESP-IDF 4.x vs 5.x due to incompatible APIs:

```cpp
// drivers/rmt/clockless_rmt_esp32.h
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
    // ESP-IDF 4.x path
    #include "rmt_4/idf4_clockless_rmt_esp32.h"
#else
    // ESP-IDF 5.x path
    #include "rmt_5/idf5_clockless.h"  // ChannelBusManager-based
#endif
```

### Why Separate rmt_4/ and rmt_5/?

**Incompatible APIs**: ESP-IDF 5.x completely redesigned the RMT peripheral API. The two implementations cannot coexist.

**Different architectures**:
- **RMT4**: ISR-driven, manually refills RMT buffer during transmission
- **RMT5**: DMA-backed, asynchronous transmission via `led_strip` driver
- **RMT5 V2**: Worker pool pattern - reusable workers + ephemeral controllers

**Cannot be merged**: Attempting to unify them would create unmaintainable #ifdef spaghetti.

## Driver Selection Logic

### Compile-Time Selection

Drivers are selected via preprocessor macros, evaluated in `core/fastled_esp32.h`:

```cpp
// Priority order (first match wins):

#if defined(FASTLED_ESP32_I2S) && FASTLED_ESP32_I2S
    #include "../drivers/i2s/clockless_i2s_esp32.h"  // I2S parallel

#elif defined(FASTLED_ESP32_LCD_I80) && FASTLED_ESP32_LCD_I80
    #include "../drivers/lcd_cam/clockless_lcd_i80_esp32.h"  // LCD I80 (S3/P4)

#elif defined(FASTLED_ESP32_LCD_RGB) && FASTLED_ESP32_LCD_RGB
    #include "../drivers/lcd_cam/clockless_lcd_rgb_esp32.h"  // LCD RGB (P4)

#elif defined(FASTLED_ESP32_PARLIO) && FASTLED_ESP32_PARLIO
    #include "../drivers/parlio/clockless_parlio_esp32p4.h"  // PARLIO (P4)

#elif defined(FASTLED_ESP32_HAS_CLOCKLESS_SPI)
    #include "../drivers/spi_ws2812/clockless_spi_esp32.h"  // Clockless SPI

#elif defined(FASTLED_ESP32_HAS_RMT)
    #include "../drivers/rmt/clockless_rmt_esp32.h"  // RMT (default)

#endif
```

### Runtime Detection

Some features are detected at runtime:
- **SPI lane count**: Auto-detected based on pin configuration (1/2/4/8-lane)
- **RMT channel availability**: Queries hardware capabilities
- **DMA availability**: Automatically enabled when supported

## Hardware SPI vs Clockless SPI

The ESP32 platform has **two completely different SPI subsystems**:

### Hardware SPI (`drivers/spi/`)

**Purpose**: Drive **clocked LED chipsets** (APA102, SK9822, LPD8806, etc.)

**Key features**:
- Uses physical SPI clock line (SCK/SCLK)
- DMA-backed parallel transmission
- Multi-lane support: 1/2/4/8-lane configurations
- Zero CPU usage during transmission
- ~40× faster than software bitbang

**Files**:
- `spi_hw_1_esp32.cpp` - 1-lane SPI
- `spi_hw_2_esp32.cpp` - 2-lane SPI (dual)
- `spi_hw_4_esp32.cpp` - 4-lane SPI (quad)
- `spi_hw_8_esp32.cpp` - 8-lane SPI (octal, ESP32-P4 only)
- `spi_platform_esp32.cpp` - Platform abstraction
- `spi_output_template.h` - Template for SPI output
- `spi_device_proxy.h` - SPI device proxy

**When to use**: LEDs with clock lines (APA102, SK9822, LPD8806, etc.)

### Clockless SPI (`drivers/spi_ws2812/`)

**Purpose**: Emulate **WS2812 timing protocol** using SPI peripheral as a timing hack

**Key features**:
- SPI clock hidden (not connected externally)
- Data line creates precisely-timed pulses matching WS2812 protocol
- Only supports WS2811/WS2812/WS2812B timing
- Cannot support other clockless LEDs (SK6812, APA106, etc.)

**Files**:
- `clockless_spi_esp32.h` - WS2812-over-SPI driver
- `strip_spi.h` / `strip_spi.cpp` - SPI strip implementation

**When to use**: WS2812-style LEDs when RMT channels are exhausted

**Limitations**: Fixed timing (cannot support SK6812, APA106, etc. due to SPI constraints)

## Key Design Decisions

### 1. No Target Directories

**Decision**: Use inline `#ifdef CONFIG_IDF_TARGET_*` within files rather than separate directories per target.

**Rationale**:
- 90%+ of code is target-agnostic
- Pin mappings are the primary difference
- Reduces code duplication
- Easier to maintain cross-target features

**When might this change?**: If target-specific code exceeds 100 lines per file, consider extracting to target-specific files.

### 2. No Environment Directories (Arduino vs ESP-IDF)

**Decision**: Handle Arduino vs ESP-IDF differences inline with minimal `#ifdef ARDUINO`.

**Rationale**:
- Differences are typically <10 lines per file (e.g., `#include <Arduino.h>` vs `#include "esp_system.h"`)
- Arduino-ESP32 framework uses ESP-IDF underneath, so most APIs are identical
- Separate directories would duplicate 95% of code

**When might this change?**: If Arduino-specific code becomes substantial (e.g., >50 lines per file).

### 3. IDF Version Separation (rmt_4/ vs rmt_5/)

**Decision**: Completely separate implementations for ESP-IDF 4.x and 5.x.

**Rationale**:
- ESP-IDF 5.x completely redesigned RMT API (incompatible with 4.x)
- RMT4 uses manual ISR refill, RMT5 uses DMA + led_strip driver
- Cannot coexist in the same binary (Espressif runtime abort)
- Merging would create unmaintainable #ifdef complexity

**Not subject to change**: This separation is permanent due to API incompatibility.

### 4. Worker Pool Pattern (RMT5 V2)

**Decision**: RMT5 V2 uses persistent worker pool + ephemeral controllers.

**Rationale**:
- Solves N > K problem (more LED strips than RMT channels)
- Workers are reused across `FastLED.show()` calls
- Controllers are lightweight, created on-demand
- Eliminates channel exhaustion issues

**Files**:
- `rmt5_worker_base.h` - Base worker interface
- `rmt5_worker.h` / `rmt5_worker.cpp` - Persistent worker
- `rmt5_worker_oneshot.h` / `rmt5_worker_oneshot.cpp` - One-shot worker
- `rmt5_worker_pool.h` / `rmt5_worker_pool.cpp` - Worker pool manager
- `rmt5_controller_lowlevel.h` / `rmt5_controller_lowlevel.cpp` - Controller

### 5. Logging Control

**Decision**: Lightweight logging via `core/esp_log_control.h` to avoid heavy `vfprintf` dependency.

**Rationale**:
- ESP-IDF's `esp_log` pulls in 10-20KB of printf/vfprintf code
- Many sketches don't need logging (constrained flash/RAM)
- Conditional logging: enabled only when `SKETCH_HAS_LOTS_OF_MEMORY` is true
- Minimal error handling: `FASTLED_ESP32_MINIMAL_ERROR_HANDLING` skips logging entirely

### 6. Interrupt Handlers in Separate Directory

**Decision**: Move architecture-specific interrupt handlers to `interrupts/` directory.

**Rationale**:
- Clear separation between Xtensa LX6, LX7, and RISC-V implementations
- Interrupt handling is orthogonal to driver selection
- Easier to add new architectures (e.g., future Xtensa LX8)

## Driver Characteristics

### RMT (Remote Control Module)
- **Supported targets**: All ESP32 variants
- **Max strips**: 1-8 per channel (depends on variant)
- **Timing precision**: Excellent (hardware-timed)
- **DMA**: IDF5 only
- **Async**: IDF5 only
- **Best for**: General-purpose LED control, mixed timing requirements

### I2S (Parallel Output)
- **Supported targets**: ESP32 classic, ESP32-S3
- **Max strips**: Up to 24 simultaneously
- **Timing precision**: Excellent (hardware-timed)
- **DMA**: Yes
- **Async**: Yes
- **Constraint**: All strips must use identical timing
- **Best for**: High strip count with uniform chipsets (e.g., 20× WS2812 strips)

### LCD_CAM (I80/RGB)

The LCD_CAM driver family includes two variants using different peripherals:

#### LCD_I80 (ESP32-S3/P4)
- **Supported targets**: ESP32-S3, ESP32-P4
- **Peripheral**: LCD_CAM in I80 mode (Intel 8080 parallel interface)
- **Max strips**: Up to 16 parallel data lines
- **Encoding**: 3-word per bit (6 bytes/bit, 144 bytes/LED)
- **PCLK Range**: 1-80 MHz
- **Control Signals**: CS, DC
- **GPIO Constraints**: Flexible (avoid GPIO0, GPIO45, GPIO46 strapping pins)
- **Timing precision**: Excellent (hardware-timed)
- **DMA**: Yes
- **Async**: Yes
- **Testing Status**: ✅ Production-tested on ESP32-S3
- **Best for**: High-bandwidth parallel output on S3/P4

#### LCD_RGB (ESP32-P4 only)
- **Supported targets**: ESP32-P4 only
- **Peripheral**: RGB LCD controller (dedicated RGB parallel display peripheral)
- **Max strips**: Up to 16 parallel data lines
- **Encoding**: 4-pixel per bit (8 bytes/bit, 192 bytes/LED)
- **PCLK Range**: 1-40 MHz (optimal: 3.2 MHz for WS2812B)
- **Control Signals**: HSYNC, VSYNC, DE, DISP
- **GPIO Constraints**: **STRICT** - GPIO39-54 only (16 pins available)
  - ❌ Rejects GPIO34-38 (strapping pins - cause boot failures)
  - ❌ Rejects GPIO24-25 (USB-JTAG - prevents debugging)
- **Timing precision**: Excellent (hardware-timed)
- **DMA**: Yes
- **Async**: Yes
- **Testing Status**: ⚠️ **EXPERIMENTAL** - Code complete, untested on hardware
- **Memory Usage**: 33% higher than LCD_I80 (PSRAM recommended for >240 LEDs)
- **Reset Gap**: VSYNC front porch timing (≥50µs between frames)
- **DMA Callback**: VSYNC interrupt-driven synchronization
- **Best for**: ESP32-P4 parallel output (when hardware becomes available)
- **Documentation**: See `LCD_I80_vs_RGB_MIGRATION_GUIDE.md` for migration details

### PARLIO (Parallel IO)
- **Supported targets**: ESP32-P4 only
- **Max strips**: Parallel output (up to 16 data lines)
- **Timing precision**: Excellent (hardware-timed)
- **DMA**: Yes
- **Async**: Yes
- **Best for**: ESP32-P4 high-performance parallel output

### Hardware SPI
- **Supported targets**: All ESP32 variants (8-lane requires ESP32-P4)
- **Max strips**: 1/2/4/8-lane configurations
- **Timing precision**: N/A (clocked LEDs use external clock)
- **DMA**: Yes
- **Async**: Yes
- **Best for**: Clocked LEDs (APA102, SK9822, LPD8806) with parallel strips

### Clockless SPI (WS2812 over SPI)
- **Supported targets**: All ESP32 variants
- **Max strips**: Limited (typically 1-2)
- **Timing precision**: Good (SPI peripheral timing)
- **DMA**: Partial
- **Async**: No
- **Constraint**: Only WS2811/WS2812 timing supported
- **Best for**: WS2812 LEDs when RMT channels are exhausted

## File Count Summary

- **Core files**: 10 files
- **Driver files**: 52 files
  - RMT: 21 files (1 dispatcher + 8 rmt_4 + 12 rmt_5)
  - I2S: 5 files
  - LCD_CAM: 12 files
  - PARLIO: 4 files
  - Hardware SPI: 7 files
  - Clockless SPI: 3 files
- **Interrupt handlers**: 5 files
- **Root-level files**: 4 files (platform abstraction)
- **Audio/OTA**: Pre-existing subdirectories

**Total**: ~71 files organized into logical hierarchy

## Include Path Examples

### From User Code (Arduino sketch)
```cpp
#include <FastLED.h>  // Top-level include (finds ESP32 automatically)
```

### From Core Aggregator (`core/fastled_esp32.h`)
```cpp
#include "../drivers/rmt/clockless_rmt_esp32.h"  // RMT dispatcher
#include "../drivers/i2s/clockless_i2s_esp32.h"  // I2S driver
#include "../drivers/spi_ws2812/clockless_spi_esp32.h"  // Clockless SPI
```

### From Driver Files (e.g., `drivers/rmt/clockless_rmt_esp32.h`)
```cpp
#include "rmt_4/idf4_clockless_rmt_esp32.h"  // Relative (same parent)
#include "rmt_5/idf5_clockless.h"  // Relative (same parent)
```

### From Global Headers (e.g., `src/fastspi.h`)
```cpp
#include "platforms/esp/32/drivers/spi/spi_device_proxy.h"  // Absolute from src/
#include "platforms/esp/32/drivers/spi/spi_output_template.h"
```

## Backward Compatibility

### Old Includes (Before Refactoring)
```cpp
#include "platforms/esp/32/fastled_esp32.h"  // ❌ Old path
#include "platforms/esp/32/clockless_rmt_esp32.h"  // ❌ Old path
```

### New Includes (After Refactoring)
```cpp
#include "platforms/esp/32/core/fastled_esp32.h"  // ✅ New path
#include "platforms/esp/32/drivers/rmt/clockless_rmt_esp32.h"  // ✅ New path
```

**Migration**: All includes were updated during the refactoring. User code remains unchanged (uses `#include <FastLED.h>`).

## Testing & Validation

### Compilation Validation
- **ESP32 classic**: ✅ Compiles (RMT default)
- **ESP32-S3**: ✅ Compiles (RMT default, I2S available, LCD_CAM available)
- **ESP32-C3**: ✅ Compiles (RMT default, RISC-V)
- **ESP32-P4**: ⚠️ Limited (PARLIO, 8-lane SPI, LCD RGB)

### Test Coverage
- **C++ unit tests**: All tests pass (driver logic validated)
- **Compilation tests**: Multi-platform validation (ESP32, S3, C3)
- **QEMU tests**: Runtime validation on ESP32 emulator

### Known Limitations
- ESP32-P4 support limited (hardware not widely available yet)
- QEMU test infrastructure has path issues (merged.bin location)
- Some advanced drivers (LCD_CAM, PARLIO) not fully tested in CI

## Future Enhancements

### Potential Improvements
1. **Common driver utilities**: Extract shared code to `drivers/common/`
2. **Target directories**: Only if target-specific code exceeds 100 lines per file
3. **Environment separation**: Only if Arduino-specific code becomes substantial
4. **Enhanced testing**: Expand QEMU coverage to all drivers

### Not Planned
- **Merging rmt_4 and rmt_5**: Incompatible APIs, intentionally separate
- **Single-file drivers**: Current organization is already maintainable
- **Removing architecture-specific files**: Necessary for Xtensa vs RISC-V differences

## References

- **ESP32 README**: `src/platforms/esp/32/README.md` - Driver selection, build flags
- **RMT5 Research**: `src/platforms/esp/32/drivers/rmt/rmt_5/RMT_RESEARCH.md` - RMT5 internals
- **LCD Implementation**: `src/platforms/esp/32/drivers/lcd_cam/implementation_notes.md` - LCD_CAM details
- **LCD_RGB Research**: `src/platforms/esp/32/drivers/lcd_cam/RGB_LCD_RESEARCH.md` - RGB LCD peripheral analysis
- **LCD Migration Guide**: `src/platforms/esp/32/drivers/lcd_cam/LCD_I80_vs_RGB_MIGRATION_GUIDE.md` - I80→RGB migration
- **LCD_RGB Release Notes**: `src/platforms/esp/32/drivers/lcd_cam/LCD_RGB_RELEASE_NOTES.md` - Feature announcement
- **PARLIO Design**: `src/platforms/esp/32/ESP32-P4_PARLIO_DESIGN.md` - PARLIO architecture
- **Interrupt Research**: `src/platforms/esp/32/interrupts/INVESTIGATE.md` - ISR implementation notes

## Summary

The ESP32 platform architecture uses a hierarchical structure with:
- **Clear separation**: Core vs drivers vs interrupt handlers
- **Conditional compilation**: Target and IDF version branching
- **Driver diversity**: 6 different driver types for different use cases
- **Backward compatibility**: Zero functional changes, pure refactoring
- **Maintainability**: Easy to find, understand, and modify driver code

This organization makes it straightforward for developers to:
- Find driver implementations quickly
- Understand driver selection process
- Add new drivers or modify existing ones
- Navigate the codebase efficiently
- Contribute to the project

**Version**: 1.1
**Last Updated**: 2025-11-05
**Status**: Complete (Post-Refactoring + LCD_RGB Documentation)
