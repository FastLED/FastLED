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
