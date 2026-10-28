# FastLED Platform: ESP8266

ESP8266 family support (Xtensa).

## Files (quick pass)
- `fastled_esp8266.h`: Aggregator; includes pin/SPI and clockless.
- `fastpin_esp8266.h`: Direct GPIO helpers and pin mappings (raw/NodeMCU/D1 variants).
- `fastspi_esp8266_hspi.h`: Native register-level HSPI driver for the hardware SPI pair (no Arduino `<SPI.h>`); also exposed to the channel API as `Bus::SPI`.
- `spi_hspi_clock.h`: Pure C++11 constexpr SPI1 clock-divider math (host-testable).
- `spi_output_template.h`: `SPIOutput<>` dispatch: native HSPI vs. software bit-bang.
- `clockless_esp8266.h`: Single‑lane clockless driver using cycle counter with retry/abort logic for NMI/ISR windows.
- `clockless_block_esp8266.h`: Multi‑lane “blockless” output via transposition and timed toggles.
- `led_sysdefs_esp8266.h`: System defines for ESP8266.
- `progmem_esp8266.h`: PROGMEM helpers for ESP8266.

## Behavior & timing
- Uses `rsr ccount` cycle counter for tight timing; NMIs can force frame aborts/retries to preserve first‑pixel integrity.
- Multi‑lane variant limits usable lanes; ensure pin mask and lane count align with board wiring.

Notes:
- Typical settings: `FASTLED_USE_PROGMEM=0`, `FASTLED_ALLOW_INTERRUPTS=1`; long ISRs will impact signal quality.
- `FASTLED_USE_PROGMEM=0` does **not** mean constants live in RAM here — see below.

## Optional feature defines

- **`FASTLED_USE_PROGMEM`**: Default `0` in `led_sysdefs_esp8266.h`, but **vestigial on this platform** — it does not control data placement.

  `fastled_progmem.h` dispatches on platform before testing the flag: its `#elif defined(ESP8266)` arm includes `progmem_esp8266.h`, which maps `FL_PROGMEM` to real `PROGMEM` and `FL_PGM_READ_*` to real `pgm_read_*`. The `#if (FASTLED_USE_PROGMEM == 1)` branch lives in an `#else` that ESP8266 never reaches.

  So FastLED tables — palettes, gamma/sine LUTs, noise permutation tables — are flash-resident regardless. A `DEFINE_GRADIENT_PALETTE` links into `.irom0.text` (`0x402xxxxx`), not DRAM.

  Setting it to `1` is a compile error (`platforms/esp/compile_test.hpp`), and would change nothing if it weren't. See [#743](https://github.com/FastLED/FastLED/issues/743).
- **`FASTLED_ALLOW_INTERRUPTS`**: Allow interrupts during show. Default `1` in `led_sysdefs_esp8266.h`.
- **`FASTLED_INTERRUPT_RETRY_COUNT`**: Max retries when a frame aborts due to long ISRs/NMIs. Default `2` (see `fastled_config.h`). Used by both single-lane and block drivers.

- **Pin order selection** (choose one before including `FastLED.h`):
  - **`FASTLED_ESP8266_RAW_PIN_ORDER`**: Use raw GPIO numbering. This is the default. Arduino board constants such as NodeMCU `D5` already expand to raw GPIO numbers and work with this mode.
  - **`FASTLED_ESP8266_NODEMCU_PIN_ORDER`**: Legacy NodeMCU board-label numbering for bare numeric pins (for example, `5` means D5/GPIO14). Do not use this mode with Arduino `Dn` constants, because they have already been mapped to GPIO numbers.
  - **`FASTLED_ESP8266_D1_PIN_ORDER`**: Legacy Wemos D1-style mapping for bare numeric pins.

- **SPI backend**
  - **GPIO13/GPIO14 (MOSI/SCK)**: clocked chipsets such as APA102 use the native register-level HSPI driver (`fastspi_esp8266_hspi.h`) by default. It writes `SPI1W0..15`/`SPI1CMD` directly, honors the chipset's `SPI_SPEED`, and does not depend on Arduino `<SPI.h>`. The pads are fixed by the peripheral, so the dispatch resolves the active pin-order mode first: 13/14 in raw order, 7/5 in NodeMCU and D1 order (those are GPIO13/GPIO14). Using GPIO13/14 this way takes the pads away from Arduino `SPI`, so a sketch cannot also run `SPI` on those pins while FastLED drives them. The channel API reaches the same engine through `fl::Bus::SPI` (`hspi_bus_traits.h`), selected by template with no define — add a clocked strip on 13/14 with `add<SpiChipsetConfig>()` or let it win over the bit-bang fallback via priority.
  - **Any other pin pair**: falls back to the shared software SPI bit-bang (`Bus::BIT_BANG`).
  - **`FASTLED_ALL_PINS_HARDWARE_SPI`**: **retired (#4942).** The legacy Arduino-SPI shim it enabled (`fastspi_esp8266.h`) is gone; the shared software bit-bang now covers every non-HSPI pin pair, and the hardware HSPI pair is selected by template. The define no longer has any effect on ESP8266.

- **`FASTLED_ESP8266_UART`**: **retired (#4944).** The UART1 (GPIO2) WS2812
  driver is now available and selectable by template — through the channel API
  (`fl::Bus::UART`) or the legacy `fl::UARTController_ESP8266` controller — with
  no define. A sketch that never names the engine links no UART-driver symbols.
  UART1 TX is fixed to GPIO2, so the channel driver only claims a clockless
  channel whose data pin is GPIO2; it is not registered by
  `FastLED.enableAllDrivers()`, so ordinary pin-2 clockless strips keep the
  bit-bang driver unless you select `Bus::UART` explicitly.
- **`FASTLED_ESP8266_UART_BAUD`** / **`FASTLED_ESP8266_UART_RESET_US`**: tune the
  UART baud (default 3.2 Mbaud) and reset/latch time (default 300 µs).
- **`FASTLED_DEBUG_COUNT_FRAME_RETRIES`**: When defined, counts and reports frame retries due to timing interference during show(). Helpful for diagnosing ISR contention.

Place these defines before including `FastLED.h` in your sketch.
