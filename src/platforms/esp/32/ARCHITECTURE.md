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
    └── isr_esp32.cpp            # ISR API implementation (IDF 5.0+)
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
    │       │     ├── idf4_clockless_rmt_esp32.h (ChannelManager-based)
    │       │     └── channel_engine_rmt4.h / channel_engine_rmt4.cpp
    │       │
    │       └─→ rmt_5/ (ESP-IDF 5.x - default, DMA-backed)
    │             ├── idf5_clockless.h (ChannelManager-based)
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
    │       └─→ parlio_engine.h / parlio_engine.cpp.hpp
    │
    └─→ drivers/spi_ws2812/clockless_spi_esp32.h (WS2812 over SPI)
            ↓
            └─→ strip_spi.h / strip_spi.cpp
```

## Stock Arduino USB ownership

Stock Arduino CDC startup remains supported without changing core flags merely
to include FastLED. Unless native USB is explicitly selected, FastLED uses SDK
UART while application `Serial` retains its framework-owned USB route. Existing
FBuild USB profiles explicitly select native ownership and retain their RPC
transport. See [stock console integration](../../../../docs/ESP32_STOCK_CONSOLE_INTEGRATION.md)
for configuration, footprint accounting and physical validation limits.

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
    #include "rmt_5/idf5_clockless.h"  // ChannelManager-based
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

**Decision**: Use native Espressif APIs on every ESP32 variant, under both
Arduino-ESP32 and bare ESP-IDF. Select the required SDK capability, such as
`FL_HAS_INCLUDE("driver/gpio.h")`, at the platform dispatch boundary.

**Rationale**:
- Arduino-ESP32 bundles ESP-IDF, so its presence does not require Arduino APIs
- Selecting SDK drivers directly keeps peripheral ownership and linker dependencies explicit
- Separate directories would duplicate 95% of code

**Rule: ESP platform source must not include or probe for `Arduino.h` or
`WProgram.h`, or include `platforms/arduino/io_arduino.hpp`.** There is no
Arduino-backend opt-in exception. `EspArduinoHeaderChecker` enforces this through
normal `./lint`, including inactive preprocessor branches. A header-availability
probe for Arduino is itself an unwanted dependency decision, even if the native
branch would usually be selected. Application sketches may still include
`Arduino.h` and explicitly use Arduino `Serial`; this policy governs FastLED's
ESP implementation.

`pin_esp32_native.hpp`'s SDK capability checks remain the reference pattern.
Keep chip selection at the preprocessor boundary and select concrete C++ types
for shared behavior; do not duplicate framework-specific implementations.

Two categories of exception to the "no `defined(ARDUINO)`" rule are expected and fine, and don't need converting:

1. **Platform identification**, not dispatch: `is_esp.h`'s `ARDUINO_ARCH_ESP32`/`ARDUINO_ARCH_ESP8266` checks identify which chip family is compiling, not which framework/API surface to call.
2. **Host-mock selection**: files under `drivers/*/​*_mock.cpp.hpp` and their matching `channel_driver_*.cpp.hpp` dispatchers guard on `!defined(ARDUINO) && (linux/apple/win)` to select the host test-double vs the real device implementation — that's choosing device-vs-host, not Arduino-vs-IDF, and both device-side arms already dispatch on capability detection internally.

#### Native console transports

- **UART:** use the ESP-IDF UART driver. Console interrupts use level 1 so
  timing-sensitive RMT level-3 interrupts can preempt console traffic.
- **Hardware USB Serial/JTAG:** use the SDK USB Serial/JTAG driver when the
  selected chip and board route support that peripheral. Set
  `FASTLED_ESP_FORCE_USB_SERIAL_JTAG=1` and disable
  `ARDUINO_USB_CDC_ON_BOOT` globally, removing any board default that enables
  it. Arduino HWCDC installs a separate interrupt handler and RX queue; it
  cannot share the native driver. Flag changes must apply to the core build,
  not just a sketch header.
- **TinyUSB CDC:** this is a separate USB peripheral path, not a synonym for
  hardware USB Serial/JTAG. Native ownership is explicit with
  `FL_ESP_USE_TINYUSB_CDC=1`. Set all Arduino USB startup flags to zero:
  `ARDUINO_USB_CDC_ON_BOOT`, `ARDUINO_USB_MSC_ON_BOOT`,
  `ARDUINO_USB_DFU_ON_BOOT`, and `ARDUINO_USB_ON_BOOT`. Arduino's automatic CDC
  startup installs callbacks that consume incoming bytes, so a second native
  reader cannot safely share that ownership. Supply `USB_VID`/`USB_PID` from
  the board build configuration; identities must already exist in the
  [FastLED boards registry](https://github.com/FastLED/boards), following
  [the registry policy](../../../../agents/docs/usb-vid-pid-registry.md).

Board USB-mode flags are configuration inputs; they do not authorize Arduino
API calls. Compile checks and UART RPC tests do not establish native USB
enumeration, RX/TX, disconnect, or reconnect behavior: verify those on the
corresponding physical USB transport before claiming hardware support.

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
- Conditional logging: enabled only when `SKETCH_HAS_LARGE_MEMORY` is true
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
- Espressif QEMU does not model every LED peripheral interrupt; CI validates
  build, boot, and driver/channel registration, while transmission requires HIL
- Advanced-driver electrical output (LCD_CAM, PARLIO) still requires HIL

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

### Optional console writer without Arduino headers

ESP output checks the weak C symbol `fastled_esp_console_write` before using
its native writer. A weak reference does not extract an adapter or Arduino
Serial object from an archive. An application can provide the function in an
object it already links, without exposing Arduino types to FastLED:

```cpp
// Application code; initialize Serial in setup before using it.
#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>

extern "C" size_t fastled_esp_console_write(const uint8_t* bytes,
                                            size_t size) noexcept {
    if (!Serial) return static_cast<size_t>(-1); // Use native output.
    return Serial.write(bytes, size);
}
```

Return the accepted byte count, or `size_t(-1)` to choose native output. A
partial count is not retried through a different writer. The provider must not
call FastLED's console functions recursively. This hook redirects output only;
console initialization and input retain their selected backend.

Native UART and Arduino `Serial.write(buffer, size)` already share the SDK
per-port TX mutex. FastLED assembles each `println` body and newline into one
write so another complete-buffer writer cannot split it. Separate application
`print` calls remain separate records; a private FastLED mutex cannot make
those calls atomic against arbitrary Arduino output. USB backend chunking has
its own transport limits, so this UART guarantee is not a USB frame guarantee.

For older Arduino 2.x cores before 2.0.17, an address-only weak reference detects
an already linked `HardwareSerial` object. Only that coexistence case initializes
the small C UART HAL before installing a fresh console driver, avoiding the old
core's null-mutex crash on a later `Serial.begin()`. Preinstalled ports keep
their configuration. No class layout or C++ method ABI is assumed; native-only
sketches do not acquire a strong dependency on Arduino's Serial object.

The native TinyUSB driver currently provides a CDC-only device. Applications
using composite USB classes must migrate those classes with the device-stack
owner. Arduino's software upload/reset handshake is not provided by this
backend; use the board's ROM download/BOOT procedure when necessary. Neither
compilation nor UART AutoResearch establishes USB enumeration or reset parity.

Packaged TinyUSB SDKs can enable class drivers absent from this device's CDC
configuration. Their mandatory callbacks must be supplied by the native owner
with unsupported responses; otherwise the linker may extract Arduino composite
USB owners to satisfy those references. These callbacks do not advertise or
implement HID, storage, network, or DFU interfaces. Modern Arduino startup can
also reference Serial0 directly (for example, main.cpp in core 3.3.11 calls
Serial0.setPins). Removing FastLED's references does not remove that independent
framework root; do not silently disable application Serial to hide it.


#### Console change validation

The matched 96-pixel Lolin D32 reporter, built with Arduino 2.0.17 / IDF 4.4.7,
uses 289,446 bytes flash and 23,476 bytes physical static RAM with the native
console. The previous async RMT baseline uses 293,006 / 23,716 bytes: a saving
of 3,560 flash and 240 static RAM bytes. No executable HardwareSerial or
Arduino String functions remain in the native reporter. This does not remove
the entire regression relative to 3.10.3 (253,022 / 22,216 bytes).

Physical classic ESP32 validation uses the canonical AutoResearch
`esp32dev_idf44` target (Arduino 2.0.3 / IDF 4.4.1). Native RPC and three RMT
loopback cases pass. Both initialization orders, real baud changes to 57,600
and back to 115,200, and the optional writer hook pass six concurrent-output
batches: 240 exact 192-byte records, with heap integrity before and after every
batch. The provider case records exactly 20 adapter calls. These measurements
cover UART hardware; native USB still requires physical enumeration and
reconnection validation on each route.


The clean native sanitizer suite passes 326 unit targets and 95 examples.
Normal `bash lint` passes. The previous console dispatcher produces two
`EspArduinoHeaderChecker` violations when checked as an isolated source copy;
the current dispatcher passes. No additional CI workflow is needed.
