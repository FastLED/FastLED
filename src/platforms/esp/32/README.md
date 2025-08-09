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

## Optional feature defines

- **`FASTLED_USE_PROGMEM`**: Control PROGMEM usage on ESP32. Default `0` in `led_sysdefs_esp32.h`.
- **`FASTLED_ALLOW_INTERRUPTS`**: Allow interrupts during show. Default `1` in `led_sysdefs_esp32.h`.
- **`FASTLED_ESP32_RAW_PIN_ORDER`**: Pin-order override hook defined in `led_sysdefs_esp32.h` (left undefined by default so the platform headers can choose). Set if you need raw GPIO numbering.

- **Backend selection**
  - **`FASTLED_ESP32_I2S`**: Enable I2S-parallel backend. Good for many strips with identical timing. See `clockless_i2s_esp32.h`.
  - **`FASTLED_ESP32_USE_CLOCKLESS_SPI`**: Force the WS2812-over-SPI path instead of RMT when available. Only supports WS2811/WS2812 timings. See `fastled_esp32.h` and `clockless_spi_esp32.h`.
  - The normal default is RMT when available (`FASTLED_ESP32_HAS_RMT` comes from `third_party/espressif/led_strip/src/enabled.h`).

- **SPI tuning (clocked LEDs and WS2812-over-SPI path)**
  - **`FASTLED_ALL_PINS_HARDWARE_SPI`**: Use the hardware SPI driver. See notes in `fastspi_esp32.h`.
  - **`FASTLED_ESP32_SPI_BUS`**: Select SPI bus: `VSPI`, `HSPI`, or `FSPI`. Defaults per target: S2/S3 use `FSPI`, others default to `VSPI` (see `fastspi_esp32.h`).
  - **`FASTLED_ESP32_SPI_BULK_TRANSFER`**: When `1`, batches pixels into blocks to reduce transfer overhead and improve throughput at the cost of RAM. Default `0`.
  - **`FASTLED_ESP32_SPI_BULK_TRANSFER_SIZE`**: Bulk block size (CRGBs) when bulk mode is enabled. Default `64`.

- **RMT (IDF4 path in `rmt_4/`)**
  - **`FASTLED_RMT_BUILTIN_DRIVER`**: Use the ESP-IDF built-in `rmt` driver (prebuilds full symbol buffer) instead of FastLED’s incremental ISR driver. Reduces flicker but uses much more RAM. Default `false`. Not supported on IDF5 (see warning in `rmt_5/idf5_rmt.cpp`).
  - **`FASTLED_RMT_SERIAL_DEBUG`**: When `1`, print RMT errors to Serial for debugging. Default `0`.
  - **`FASTLED_RMT_MEM_WORDS_PER_CHANNEL`**: Override RMT words per channel. Defaults to `SOC_RMT_MEM_WORDS_PER_CHANNEL` on newer IDF or `64` on older.
  - **`FASTLED_RMT_MEM_BLOCKS`**: Number of RMT memory blocks per channel to use. Default `2`. Higher values reduce refill interrupts but consume more shared memory and reduce usable channels per group.
  - **`FASTLED_RMT_MAX_CONTROLLERS`**: Max FastLED controllers the driver can queue. Default `32`.
  - **`FASTLED_RMT_MAX_CHANNELS`**: Max TX channels to use. Defaults to SoC capability (e.g., `SOC_RMT_TX_CANDIDATES_PER_GROUP`) or chip-specific constants. Reduce to reserve channels for other RMT users.
  - **`FASTLED_RMT_MAX_TICKS_FOR_GTX_SEM`**: Max RTOS ticks to wait for the RMT TX semaphore before bailing out. Default `portMAX_DELAY`. Example to cap at ~2s: `#define FASTLED_RMT_MAX_TICKS_FOR_GTX_SEM (2000/portTICK_PERIOD_MS)`.
  - **`FASTLED_ESP32_FLASH_LOCK`**: When set to `1`, blocks flash operations during show to avoid timing disruptions. See comments in `rmt_4/idf4_rmt.h` and usage in `idf4_rmt_impl.cpp`.
  - (Not currently used) **`FASTLED_RMT_SHOW_TIMER`**: Timer debug toggle (commented out in `rmt_4/idf4_rmt.h`).
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
