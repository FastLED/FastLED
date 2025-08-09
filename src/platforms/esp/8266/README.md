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

- `FASTLED_ALL_PINS_HARDWARE_SPI` (defined): Enable the ESP8266 hardware SPI backend for clocked LEDs; see `fastspi_esp8266.h`.
- Pin order selection (choose one):
  - `FASTLED_ESP8266_RAW_PIN_ORDER`
  - `FASTLED_ESP8266_NODEMCU_PIN_ORDER`
  - `FASTLED_ESP8266_D1_PIN_ORDER`
  If none is defined, NodeMCU order is selected when `ARDUINO_ESP8266_NODEMCU` is set, else RAW.
- `FASTLED_ALLOW_INTERRUPTS` (0|1): Allow brief interrupt windows during show. Default 1; `INTERRUPT_THRESHOLD` default 0.
- `FASTLED_USE_PROGMEM` (0|1): Control PROGMEM usage on ESP8266. Default 0.
- `FASTLED_DEBUG_COUNT_FRAME_RETRIES` (defined): Enable `_frame_cnt`/`_retry_cnt` counters in clockless drivers for retry diagnostics.
- `FASTLED_INTERRUPT_RETRY_COUNT` (int): Number of retries after ISR/NMI preemption before abandoning the frame in clockless paths.

Notes:
- `FASTLED_HAS_CLOCKLESS`/`FASTLED_HAS_BLOCKLESS` are provided by the implementation and not user-set.
