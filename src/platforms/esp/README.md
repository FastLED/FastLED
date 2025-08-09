# FastLED Platform: esp

ESP platform support covering ESP8266 and ESP32 (Xtensa and RISC‑V variants), with multiple output backends (RMT, I2S‑parallel, and clockless SPI for WS2812‑class LEDs).

## Top‑level files (quick pass)
- `compile_test.hpp`: Compile‑time assertions for ESP8266/ESP32 flags (PROGMEM usage, interrupts policy, F_CPU, architecture guards, millis availability, driver feature checks).
- `esp_assert.h`: Lightweight `FASTLED_ASSERT` macro that prints via `fl::StrStream` without pulling heavy logging.
- `esp_version.h`: IDF version detection/fallback macros for toolchains lacking `esp_idf_version.h`.
- `int.h`: ESP‑friendly integer/pointer typedefs in `fl::`.
- `io_esp.h`: Minimal I/O helpers; on ESP32 uses direct `fputs` to stdout to avoid heavy logging.
- `print_esp.h`: ESP32 UART helpers using `driver/uart.h` to `uart_write_bytes()`.
- `esp_compile.hpp`: Hierarchical include hook for ESP; sources are brought in via build system globs.

## Subdirectories
- `32/` (ESP32 family)
  - `fastled_esp32.h`: Aggregator; selects one of the clockless drivers based on IDF features/macros.
  - Clockless drivers:
    - `clockless_rmt_esp32.h`: RMT backend; dispatches to `rmt_4/` (IDF4) or `rmt_5/` (IDF5) implementations.
    - `clockless_i2s_esp32.h`: I2S‑parallel backend; can drive up to 24 strips in parallel (same timings).
    - `clockless_spi_esp32.h`: Clockless over SPI pipeline for WS2812‑class LEDs (uses `spi_ws2812/`).
  - RMT5 interface: `rmt_5/idf5_rmt.h` and helpers.
  - I2S support: `i2s/` setup and DMA buffer management invoked by the I2S driver.
  - Pin/SPI headers: `fastpin_esp32.h`, `fastspi_esp32.h` (included conditionally).

- `8266/` (ESP8266 family)
  - `clockless_esp8266.h`: Single‑lane clockless controller using cycle counter with interrupt handling and frame retry handling.
  - `clockless_block_esp8266.h`: Multi‑lane “blockless” controller (transpose + timed output across lanes).
  - `fastpin_esp8266.h`: Direct GPIO access helpers and pin mappings for ESP8266 variants and NodeMCU/D1 mappings.
  - `fastspi_esp8266.h`: Hardware SPI helper (where available) for ESP8266.

## Backend selection quick guide

Pick one backend explicitly via preprocessor defines, then include `fastled_esp32.h`:

- RMT (recommended for most projects): define `FASTLED_ESP32_HAS_RMT`.
  - IDF 4.x uses `rmt_4/`, IDF 5.x uses `rmt_5/` automatically via `ESP_IDF_VERSION`.
  - Best choice when strips have differing timings or when you want per‑channel independence.
- I2S parallel (many strips, same timing): define `FASTLED_ESP32_I2S`.
  - Drives up to 24 strips in parallel with identical WS281x timings. All strips must share the same protocol/timing.
  - Good for LED matrices/walls when memory for DMA buffers is available.
- Clockless over SPI (WS2812‑class only): define `FASTLED_ESP32_HAS_CLOCKLESS_SPI`.
  - Uses SPI to approximate WS2812 timings; lower CPU overhead than bit‑bang, but limited to WS2812‑compatible LEDs.

IDF version notes:

- `esp_version.h` provides a robust `ESP_IDF_VERSION` macro shim so you can write portable conditionals.
- RMT API differs substantially between IDF 4 and 5; the dispatcher inside `clockless_rmt_esp32.h` selects the correct layer.

Typical pitfalls:

- Long ISRs (Wi‑Fi, BLE, heavy logging) can introduce jitter. Prefer RMT or I2S if your application has significant background activity.
- I2S requires uniform timings across lanes. If you mix device types (e.g., WS2812B and SK6812), prefer RMT.
- For high throughput, reduce logging (see `esp_log_control.h`) and pin RMT tasks to a core with fewer interrupts.

## Behavior and constraints
- ESP32 driver selection: `FASTLED_ESP32_HAS_RMT` vs `FASTLED_ESP32_HAS_CLOCKLESS_SPI` (or `FASTLED_ESP32_I2S`) determines backend; `fastled_esp32.h` chooses based on macros/IDF version.
- ESP8266 path uses cycle‑counter‑based timing; NMIs and interrupt durations can force frame retries or early aborts to maintain signal quality.
