# FastLED Platform: ESP8266

ESP8266 family support (Xtensa).

## Files (quick pass)
- `fastled_esp8266.h`: Aggregator; includes pin/SPI and clockless.
- `fastpin_esp8266.h`: Direct GPIO helpers and pin mappings (raw/NodeMCU/D1 variants).
- `fastspi_esp8266.h`: SPI backend (where available).
- `clockless_esp8266.h`: Single‑lane clockless driver using cycle counter with retry/abort logic for NMI/ISR windows.
- `clockless_block_esp8266.h`: Multi‑lane “blockless” output via transposition and timed toggles.
- `led_sysdefs_esp8266.h`: System defines for ESP8266.
- `progmem_esp8266.h`: PROGMEM helpers for ESP8266.

## Behavior & timing
- Uses `rsr ccount` cycle counter for tight timing; NMIs can force frame aborts/retries to preserve first‑pixel integrity.
- Multi‑lane variant limits usable lanes; ensure pin mask and lane count align with board wiring.

Notes:
- Typical settings: `FASTLED_USE_PROGMEM=0`, `FASTLED_ALLOW_INTERRUPTS=1`; long ISRs will impact signal quality.

## Optional feature defines

- **`FASTLED_USE_PROGMEM`**: Control PROGMEM usage. Default `0` in `led_sysdefs_esp8266.h`.
- **`FASTLED_ALLOW_INTERRUPTS`**: Allow interrupts during show. Default `1` in `led_sysdefs_esp8266.h`.
- **`FASTLED_INTERRUPT_RETRY_COUNT`**: Max retries when a frame aborts due to long ISRs/NMIs. Default `2` (see `fastled_config.h`). Used by both single-lane and block drivers.

- **Pin order selection** (choose one before including `FastLED.h`):
  - **`FASTLED_ESP8266_RAW_PIN_ORDER`**: Use raw GPIO numbering.
  - **`FASTLED_ESP8266_NODEMCU_PIN_ORDER`**: Use NodeMCU D0/D1 labeling.
  - **`FASTLED_ESP8266_D1_PIN_ORDER`**: Use Wemos D1-style mapping.
  - If none are defined, the code defaults to NodeMCU on `ARDUINO_ESP8266_NODEMCU`, otherwise raw pin order.

- **SPI backend**
  - **`FASTLED_ALL_PINS_HARDWARE_SPI`**: Route clocked LED chipsets via the hardware SPI driver. See `fastspi_esp8266.h` notes.

- **Debugging / diagnostics**
  - **`FASTLED_DEBUG_COUNT_FRAME_RETRIES`**: When defined, counts and reports frame retries due to timing interference during show(). Helpful for diagnosing ISR contention.

Place these defines before including `FastLED.h` in your sketch.
