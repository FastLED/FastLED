# FastLED Platform: ESP32 (Xtensa/RISC‑V)

ESP32 family support with multiple clockless backends.

## Aggregator & sysdefs
- `fastled_esp32.h`: Chooses backend based on IDF version/features/macros: RMT (IDF4/IDF5), I2S‑parallel, or clockless SPI.
- `led_sysdefs_esp32.h`: ESP32 system defines (architecture flags, millis support, interrupt policy).
- `esp_log_control.h`: Lightweight logging control (avoid heavy vfprintf dependency).

## Backends
- RMT (recommended):
  - `clockless_rmt_esp32.h`: Common entry; dispatches to IDF4/IDF5 layers.
  - `rmt_4/`: IDF4 RMT implementation.
  - `rmt_5/`: IDF5 RMT implementation and `strip_rmt.*` glue.
- I2S parallel (many strips, same timing):
  - `clockless_i2s_esp32.h`: I2S driver; DMA double‑buffering, transposition/encoding.
  - `clockless_i2s_esp32s3.*`: S3 variant hooks.
  - `i2s/i2s_esp32dev.*`: I2S helpers.
- Clockless over SPI (WS2812 only):
  - `clockless_spi_esp32.h`: SPI staging for WS2812‑class LEDs using `spi_ws2812/`.
  - `spi_ws2812/`: SPI strip driver.

## Pins, SPI, timing
- `fastpin_esp32.h`: Pin helpers for direct GPIO.
- `fastspi_esp32.h`: SPI backend helpers.
- `clock_cycles.h`: Timing helpers.

## Behavior & selection
- Define one of: `FASTLED_ESP32_HAS_RMT`, `FASTLED_ESP32_HAS_CLOCKLESS_SPI`, or `FASTLED_ESP32_I2S`.
- `ESP_IDF_VERSION` (from `esp_version.h`) determines RMT4 vs RMT5.
- I2S requires identical timing across all strips; RMT handles per‑channel timing.

Notes:
- Prefer RMT on modern IDF; I2S for high strip counts with uniform timings; SPI path only for WS2812.

## I2S support by board (parallel output)

FastLED supports the parallel I2S clockless driver on the following ESP32 targets in this tree:

- ESP32 (classic, e.g., "ESP32Dev")
  - Enable via build flags (PlatformIO `platformio.ini`):
    ```ini
    [env:esp32dev]
    platform = espressif32
    board = esp32dev
    framework = arduino
    build_flags =
      -D FASTLED_ESP32_I2S=1
      ; Optional tuning:
      ; -D FASTLED_ESP32_I2S_NUM_DMA_BUFFERS=4   ; 2–16; 4 often reduces Wi‑Fi flicker
      ; -D I2S_DEVICE=0                          ; default 0
    ```
  - Driver files: `clockless_i2s_esp32.h`, `i2s/i2s_esp32dev.*`
  - Constraints: all lanes use identical timing (same LED chipset); up to 24 lanes.

- ESP32-S3
  - Enable via build flags (PlatformIO `platformio.ini`):
    ```ini
    [env:esp32s3]
    platform = espressif32
    board = esp32-s3-devkitc-1
    framework = arduino
    build_flags =
      -D FASTLED_ESP32_I2S=1
      ; Optional:
      ; -D FASTLED_ESP32_I2S_NUM_DMA_BUFFERS=4
    ```
  - Driver files: `clockless_i2s_esp32s3.*` (wraps a dedicated S3 I2S clockless implementation)
  - Notes: requires a compatible Arduino-ESP32 core/IDF; see version checks in `clockless_i2s_esp32s3.h`.

Additional I2S defines and guidance:
- `FASTLED_I2S_MAX_CONTROLLERS` (default 24): maximum parallel lanes
- `FASTLED_ESP32_I2S_NUM_DMA_BUFFERS` (default 2): set to 4 to reduce flicker when ISRs (e.g., Wi‑Fi) are active
- All I2S lanes must share the same chipset/timings; choose RMT instead when per‑lane timing differs

## Optional feature defines

- **`FASTLED_USE_PROGMEM`**: Control PROGMEM usage on ESP32. Default `0` in `led_sysdefs_esp32.h`.
- **`FASTLED_ALLOW_INTERRUPTS`**: Allow interrupts during show. Default `1` in `led_sysdefs_esp32.h`.
- **`FASTLED_ESP32_RAW_PIN_ORDER`**: Pin-order override hook defined in `led_sysdefs_esp32.h` (left undefined by default so the platform headers can choose). Set if you need raw GPIO numbering.

- **Backend selection**
  - **`FASTLED_ESP32_I2S`**: Enable I2S-parallel backend. Good for many strips with identical timing. See `clockless_i2s_esp32.h`.
  - **`FASTLED_ESP32_USE_CLOCKLESS_SPI`**: Force the WS2812-over-SPI path instead of RMT when available. Only supports WS2811/WS2812 timings. See `fastled_esp32.h` and `clockless_spi_esp32.h`.
  - The normal default is RMT when available (`FASTLED_ESP32_HAS_RMT` comes from `platforms/esp/32/feature_flags/enabled.h`).

- **SPI tuning (clocked LEDs and WS2812-over-SPI path)**
  - **Hardware SPI is now enabled by default** on ESP32 via GPIO matrix routing. Any GPIO can be used for `DATA_PIN` and `CLOCK_PIN`.
  - **`FASTLED_ALL_PINS_HARDWARE_SPI`**: **DEPRECATED**. Hardware SPI is now the unconditional default. This define is accepted for backwards compatibility but no longer required.
  - **`FASTLED_FORCE_SOFTWARE_SPI`**: Force software bit-banging SPI instead of hardware SPI. The SPI bus manager will still handle device registration and conflict detection, but all data transmission uses software bit-banging. Use only if hardware SPI causes issues.
  - **`FASTLED_ESP32_SPI_BUS`**: Select SPI bus: `VSPI`, `HSPI`, or `FSPI`. Defaults per target: S2/S3 use `FSPI`, others default to `VSPI` (see `fastspi_esp32.h`).
  - **`FASTLED_ESP32_SPI_BULK_TRANSFER`**: When `1`, batches pixels into blocks to reduce transfer overhead and improve throughput at the cost of RAM. Default `0`.
  - **`FASTLED_ESP32_SPI_BULK_TRANSFER_SIZE`**: Bulk block size (CRGBs) when bulk mode is enabled. Default `64`.

- **RMT (IDF4 path in `rmt_4/`)**
  - **`FASTLED_RMT_SERIAL_DEBUG`**: When `1`, print RMT errors to Serial for debugging. Default `0`.
  - **`FASTLED_RMT_MEM_WORDS_PER_CHANNEL`**: Override RMT words per channel. Defaults to `SOC_RMT_MEM_WORDS_PER_CHANNEL` on newer IDF or `64` on older.
  - **`FASTLED_RMT_MEM_BLOCKS`**: Number of RMT memory blocks per channel to use. Default `2`. Higher values reduce refill interrupts but consume more shared memory and reduce usable channels per group.
  - **`FASTLED_RMT_MAX_CHANNELS`**: Max TX channels to use. Defaults to SoC capability (e.g., `SOC_RMT_TX_CANDIDATES_PER_GROUP`) or chip-specific constants. Reduce to reserve channels for other RMT users.
  - **`FASTLED_RMT4_TRANSMISSION_TIMEOUT_MS`**: Maximum time in milliseconds to wait for RMT transmission to complete before considering it stuck. Default `2000` (2 seconds). Set to `0` to disable timeout detection.
  - **`FASTLED_ESP32_FLASH_LOCK`**: When set to `1`, blocks flash operations during show to avoid timing disruptions. Default `0`. Note: Currently only supported on IDF 3.x; IDF 4.x+ support is pending.
  - (Not currently used) **`FASTLED_RMT_SHOW_TIMER`**: Timer debug toggle (legacy flag, no longer used).
  - **`FASTLED_INTERRUPT_RETRY_COUNT`**: Global retry count when timing is disrupted (also used by the blockless path). Default `2` in `fastled_config.h`.
  - **`FASTLED_DEBUG_COUNT_FRAME_RETRIES`**: When defined, enable counters/logging of frame retries due to timing issues (used in blockless/clockless drivers).

- **RMT (IDF5 path in `rmt_5/`)**
  - Uses the ESP-IDF v5 LED Strip driver through `IRmtStrip`. DMA is used automatically (`FASTLED_RMT_USE_DMA` marker macro). No user macro to select DMA mode (the driver picks it at runtime with `DMA_AUTO`).

- **I2S-parallel**
  - **`I2S_DEVICE`**: I2S device index. Default `0`.
  - **`FASTLED_I2S_MAX_CONTROLLERS`**: Max number of parallel lanes (controllers). Default `24`.
  - **`FASTLED_ESP32_I2S_NUM_DMA_BUFFERS`**: Number of I2S DMA buffers (2–16). Default `2`. Increasing to `4` often mitigates flicker during heavy interrupt activity. See `clockless_i2s_esp32.h` and `i2s/i2s_esp32dev.h`.

- **Logging / binary size**
  - **`FASTLED_ESP32_ENABLE_LOGGING`**: Controls inclusion/level of `esp_log` macros via `esp_log_control.h`. Default: enabled only when `SKETCH_HAS_LOTS_OF_MEMORY` is true; otherwise disabled to avoid pulling in heavy printf/vfprintf.
  - **`FASTLED_ESP32_MINIMAL_ERROR_HANDLING`**: When defined and logging is disabled, overrides `ESP_ERROR_CHECK` to abort without logging to keep binary size low.

- **Clock source override**
  - **`F_CPU_RMT_CLOCK_MANUALLY_DEFINED`**: If defined, sets `F_CPU_RMT` explicitly for SoCs where APB clock detection is problematic (e.g., some C6/H2 variants). Otherwise `F_CPU_RMT` derives from `APB_CLK_FREQ`.

Unless otherwise noted, all defines should be placed before including `FastLED.h` in your sketch.

## Hardware Multi-Lane SPI Support

FastLED provides hardware-accelerated multi-lane SPI for parallel LED strip control via DMA.

### Supported Configurations

**1-Lane, 2-Lane, 4-Lane (All ESP32 variants):**
- **Interfaces**: `SpiHw1`, `SpiHw2`, `SpiHw4`
- **Files**: `spi_hw_1_esp32.cpp`, `spi_hw_2_esp32.cpp`, `spi_hw_4_esp32.cpp`
- **Platforms**: ESP32, ESP32-S2, ESP32-S3, ESP32-C3, ESP32-P4
- **ESP-IDF**: 3.3+ (any version)

**8-Lane (Octal-SPI - ESP32-P4 only):**
- **Interface**: `SpiHw8`
- **File**: `spi_hw_8_esp32.cpp`
- **Platform**: ESP32-P4 only
- **ESP-IDF**: 5.0.0 or higher required
- **Pins**: Up to 8 data pins (D0-D7) + 1 clock pin

### Key Features
- **Automatic Mode Selection**: Hardware driver auto-detects lane count (1/2/4/8) based on pin configuration
- **Version Safety**: 8-lane code guarded with ESP-IDF 5.x version checks
- **Graceful Fallback**: Older platforms/IDF versions automatically fall back to software implementation
- **Zero CPU Usage**: DMA handles transmission (~40× faster than software)
- **Separate Interfaces**: Clean separation between 4-lane (`SpiHw4`) and 8-lane (`SpiHw8`)

### Expected Performance (8-Lane)
- **Speed**: ~40× faster than software octo-SPI
- **Transmission**: 8×100 LED strips in <500µs
- **CPU Usage**: 0% during transmission (DMA handles everything)

### Implementation Files
Hardware SPI implementations are organized by lane count:
- `spi_hw_1_esp32.cpp` - 1-lane (Single) SPI
- `spi_hw_2_esp32.cpp` - 2-lane (Dual) SPI
- `spi_hw_4_esp32.cpp` - 4-lane (Quad) SPI
- `spi_hw_8_esp32.cpp` - 8-lane (Octal) SPI (ESP32-P4 + IDF 5.0+)

Shared infrastructure:
- `src/platforms/shared/spi_hw_*.h` - Platform-agnostic interfaces
- `src/platforms/shared/spi_transposer.*` - Unified bit-interleaving for all widths
- `src/platforms/shared/spi_bus_manager.h` - Automatic multi-lane detection

### Testing
Comprehensive unit tests validate all lane configurations:
- 1-lane, 2-lane tests: `uv run test.py test_single_spi test_dual_spi`
- 4-lane, 8-lane tests: `uv run test.py test_quad_spi`

### Notes
- All lane configurations (1/2/4) work on all ESP32 variants
- 8-lane requires ESP32-P4 with ESP-IDF 5.0+
- Implementation is backward-compatible with existing platforms
- On unsupported platforms, software bitbang fallback is used automatically

## Multi-Lane SPI: I2S-Based 16-Lane Parallel SPI

FastLED provides automatic multi-lane SPI support for driving **up to 16 parallel SPI LED strips** simultaneously using the same `fl::Spi` API. On ESP32 platforms, this is implemented via the I2S peripheral in parallel mode.

### Implementation Details by Lane Count

| Feature | Hardware SPI (1-8 lane) | I2S-based SPI (9-16 lane) |
|---------|------------------------|---------------------------|
| **Technology** | SPI peripheral with DMA | I2S peripheral in parallel mode |
| **Max Lanes** | 8 (ESP32-P4 only) | 16 (ESP32/S2/S3) |
| **Platforms** | All ESP32 variants | ESP32, ESP32-S2, ESP32-S3 |
| **Pin Assignment** | Flexible (any GPIO) | Fixed range (GPIO 8-23) |
| **Memory** | Internal RAM | PSRAM+DMA recommended |
| **API** | `fl::Spi` class | `fl::Spi` class (same API!) |
| **Best For** | 1-8 strips, flexible pins | 9-16 strips, maximum throughput |

**Note**: The system automatically promotes to the appropriate hardware backend based on lane count. Users always use the same `fl::Spi` API.

### Supported Platforms

- **ESP32 (classic)**: 16 lanes via I2S0 parallel mode (GPIO 8-23)
- **ESP32-S2**: 16 lanes via I2S0 (GPIO 8-23)
- **ESP32-S3**: 16 lanes via I2S0 (GPIO 8-23) - **Recommended** with PSRAM
- **ESP32-C3/C6**: Not supported (only 2 I2S lanes)

### Supported LED Chipsets

Multi-lane SPI works with clock-based SPI LED chipsets:
- ✅ APA102 (tested)
- ✅ SK9822 (APA102-compatible)
- ✅ HD107S (newer, faster variant)
- ❌ WS2812/WS2811 (use the Channel API for clockless LEDs)

### Quick Example

```cpp
#include <FastLED.h>

const int CLOCK_PIN = 18;
const int DATA_PINS[] = {23, 22, 21, 19};  // 4 strips (can be up to 16!)
const int NUM_LEDS = 300;

CRGB leds[4][NUM_LEDS];

// Standard fl::Spi API - automatically uses I2S backend on ESP32
fl::Spi spi(CLOCK_PIN, DATA_PINS, fl::SPI_HW);

void setup() {
    if (!spi.ok()) {
        Serial.println("SPI init failed!");
        while(1);
    }
}

void loop() {
    // Write all 4 strips in parallel using standard API
    spi.write(
        fl::span<const uint8_t>((uint8_t*)leds[0], NUM_LEDS * 3),
        fl::span<const uint8_t>((uint8_t*)leds[1], NUM_LEDS * 3),
        fl::span<const uint8_t>((uint8_t*)leds[2], NUM_LEDS * 3),
        fl::span<const uint8_t>((uint8_t*)leds[3], NUM_LEDS * 3)
    );
    spi.wait();  // Optional: block until transmission complete
}
```

### Performance

**Throughput**: ~16× improvement over sequential SPI
- 16 strips × 300 LEDs transmitted in ~10ms (vs ~160ms sequential)
- Zero CPU usage during transmission (DMA handles everything)
- Supports up to 8K+ LEDs per strip with PSRAM

### Memory Requirements

Multi-lane SPI requires DMA-capable memory for internal buffers:
- **Small installations** (<500 LEDs/strip): Internal RAM sufficient
- **Large installations** (>500 LEDs/strip): **PSRAM strongly recommended**
- ESP32-S3 EDMA enables PSRAM to be DMA-capable (automatic)

**Enable PSRAM in PlatformIO:**
```ini
[env:esp32s3]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino
board_build.arduino.memory_type = qio_qspi
build_flags =
    -D BOARD_HAS_PSRAM
```

### Pin Configuration

**Data Pins**: Must use GPIO 8-23 (I2S parallel mode range)
- Use any subset (e.g., GPIO 23-20 for 4 strips)
- Avoid GPIO 6-11 if used for SPI flash (platform-dependent)

**Clock Pin**: Any free GPIO (recommend GPIO 18 or higher)
- Shared across all strips
- Keep wiring short for signal integrity

### Implementation Files

- Public API: `src/fl/spi.h` (unified 1-16 lane SPI API)
- 16-lane interface: `src/platforms/shared/spi_hw_16.h`
- ESP32 I2S backend: `src/platforms/esp/32/drivers/i2s/spi_hw_i2s_esp32.{h,cpp}`
- Bus manager: `src/platforms/shared/spi_bus_manager.h` (automatic promotion to 16-lane mode)

### Architecture

FastLED uses a unified SPI API (`fl::Spi`) that automatically handles 1-16 lanes:
- 1-2 lanes: Uses `SpiHw2` (dual SPI)
- 3-4 lanes: Uses `SpiHw4` (quad SPI)
- 5-8 lanes: Uses `SpiHw8` (octal SPI)
- 9-16 lanes: Uses `SpiHw16` (hexadeca SPI via ESP32 I2S)

The `SPIBusManager` automatically detects lane count and promotes to the appropriate hardware implementation.

### Documentation

For comprehensive documentation, see **`LOOP_SPI_I2S.md`** which includes:
- Hardware requirements and wiring diagrams
- Complete API reference
- Integration with fl::Spi system
- Memory optimization tips
- Troubleshooting guide
- Technical implementation details

### Using Multi-Lane SPI

The unified `fl::Spi` API works on all platforms and automatically uses hardware acceleration when available:

```cpp
#include <FastLED.h>

// Define data pins (4 strips example)
int data_pins[] = {8, 9, 10, 11};

// Create SPI device with hardware mode
fl::Spi spi(18, data_pins, fl::SPI_HW);  // Clock on GPIO 18

if (!spi.ok()) {
    Serial.println("SPI init failed");
    return;
}

// Write data to all strips
spi.write(strip0, strip1, strip2, strip3);
spi.wait();  // Block until transmission complete
```

On ESP32, when using 9-16 lanes, the `SPIBusManager` automatically:
1. Detects the lane count via `SpiHw16::getAll()`
2. Finds the I2S hardware implementation (`SpiHwI2SESP32`)
3. Promotes to hexadeca-SPI mode
4. Uses hardware-accelerated parallel transmission

## RMT backends (IDF4 vs IDF5) and how to select

Two RMT implementations exist and are selected by IDF version unless you override it:

- RMT4 (ESP‑IDF v4): legacy FastLED RMT driver in `rmt_4/`
- RMT5 (ESP‑IDF v5): uses the ESP‑IDF v5 LED Strip driver via `IRmtStrip` in `rmt_5/`

Selection rules:
- Default: `ESP_IDF_VERSION` decides. On IDF < 5, RMT5 is disabled; on IDF ≥ 5, RMT5 is enabled.
- To force RMT4 on IDF5, set this build define (PlatformIO `platformio.ini`):
  ```ini
  build_flags =
    -D FASTLED_RMT5=0
  ```
  This ensures only one RMT implementation is compiled. Do not link/use the IDF v5 `led_strip` driver in the same binary when forcing RMT4.

Important compatibility note:
- The RMT4 and RMT5 drivers cannot co‑exist in the same sketch/binary. Mixing both (e.g., using IDF v5 `led_strip` alongside FastLED’s RMT4) will trigger an Espressif runtime abort at startup.

Behavioral differences and practical guidance:
- RMT4 (IDF4)
  - Uses ISR-driven double-buffering for LED transmission with WiFi interference detection.
  - Supports time-multiplexing for >8 LED strips via channel sharing.
  - Enable `FASTLED_ESP32_FLASH_LOCK` to reduce flicker during WiFi activity (IDF 3.x only).

- RMT5 (IDF5)
  - Asynchronous, DMA‑backed LED driver. Your sketch can continue running while a frame is transmitted.
  - Recommended on modern IDF when you want concurrent work during frame output.

- I2S (parallel)
  - Also DMA‑driven; while frame data is being transferred to the peripheral, your loop can continue doing work. Use more DMA buffers (`FASTLED_ESP32_I2S_NUM_DMA_BUFFERS 4`) to improve resilience under interrupt load.

Quick PlatformIO examples

- Force RMT4 on IDF5 (legacy path):
  ```ini
  [env:esp32dev_rmt4]
  platform = espressif32
  board = esp32dev
  framework = arduino
  build_flags =
    -D FASTLED_RMT5=0
  ```

- Enable I2S on ESP32Dev or ESP32‑S3 with extra DMA buffers:
  ```ini
  [env:esp32dev_i2s]
  platform = espressif32
  board = esp32dev
  framework = arduino
  build_flags =
    -D FASTLED_ESP32_I2S=1
    -D FASTLED_ESP32_I2S_NUM_DMA_BUFFERS=4
  ```
