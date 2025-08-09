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
- Selection is auto-detected (RMT vs SPI) via `third_party/espressif/led_strip/src/enabled.h`. You can override with `FASTLED_ESP32_I2S` (force I2S) or `FASTLED_ESP32_USE_CLOCKLESS_SPI` (prefer SPI when available).
- `ESP_IDF_VERSION` (from `esp_version.h`) determines RMT4 vs RMT5.
- I2S requires identical timing across all strips; RMT handles per‑channel timing.

Notes:
- Prefer RMT on modern IDF; I2S for high strip counts with uniform timings; SPI path only for WS2812.

## Optional feature defines

- `FASTLED_ESP32_I2S` (bool): Force the I2S-parallel clockless backend. All strips must share timing. Define before including `FastLED.h`.
- `FASTLED_ESP32_USE_CLOCKLESS_SPI` (bool): Prefer the SPI-based WS2812 clockless driver when available instead of RMT.
- `FASTLED_ESP32_ENABLE_LOGGING` (0|1): Control ESP logging inclusion. Defaults to 1 only when `SKETCH_HAS_LOTS_OF_MEMORY` is true; otherwise 0. Include `esp_log_control.h` before `esp_log.h`.
- `FASTLED_ESP32_MINIMAL_ERROR_HANDLING` (defined): When logging is disabled, overrides `ESP_ERROR_CHECK` to abort without printing. Use to eliminate printf pulls.
- `FASTLED_ESP32_SPI_BUS` (VSPI|HSPI|FSPI): Select SPI bus for hardware SPI backend. Defaults: VSPI; S2/S3 coerce to FSPI for flexible pins.
- `FASTLED_ESP32_SPI_BULK_TRANSFER` (0|1): Use buffered SPI writes to reduce lock contention. Default 0.
- `FASTLED_ESP32_SPI_BULK_TRANSFER_SIZE` (int): Chunk size when bulk transfer is enabled. Default 64 bytes.
- `FASTLED_I2S_MAX_CONTROLLERS` (int): Max number of I2S controllers in parallel mode. Default 24.
- `FASTLED_ESP32_I2S_NUM_DMA_BUFFERS` (int 3..16): Number of I2S DMA buffers. If <=2 or undefined, defaults to 2.
- `I2S_DEVICE` (0|1): Select I2S peripheral index for parallel mode. Default 0.
- `FASTLED_UNUSABLE_PIN_MASK` (bitmask): Override default mask of pins FastLED should avoid on your ESP32 variant. See `fastpin_esp32.h` for defaults.
- `FASTLED_ALLOW_INTERRUPTS` (0|1): Allow brief interrupt windows during show to improve system responsiveness. Default 1; `INTERRUPT_THRESHOLD` default 0.
- `FASTLED_USE_PROGMEM` (0|1): Control PROGMEM usage on ESP32. Default 0.
- `FASTLED_DEBUG_COUNT_FRAME_RETRIES` (defined): Enable global counters `_frame_cnt` and `_retry_cnt` in clockless backends for debugging retry behavior.

Notes:
- RMT backend tuning: `FASTLED_RMT_BUILTIN_DRIVER` (IDF4 only) selects using the ESP core RMT driver (true) vs custom ISR (false, default). `FASTLED_RMT_MAX_CHANNELS` can limit TX channels; default depends on SOC caps.
- Auto-detected capabilities: `FASTLED_ESP32_HAS_RMT`, `FASTLED_ESP32_HAS_CLOCKLESS_SPI`, and `FASTLED_RMT5` come from `third_party/espressif/led_strip/src/enabled.h` and generally should not be set manually.
