# FastLED Platform: apollo3

Ambiq Apollo3 platform support.

## Files (quick pass)
- `fastled_apollo3.h`: Aggregator; includes `fastpin_apollo3.h`, `fastspi_apollo3.h`, `clockless_apollo3.h`.
- `fastpin_apollo3.h`: Pin helpers using Ambiq fastgpio; board‑specific `(pin,pad)` mappings (e.g., SFE Edge/Thing Plus/ATP, Artemis Nano, LoRa Thing Plus expLoRaBLE). Defines `HAS_HARDWARE_PIN_SUPPORT` when applicable.
- `fastspi_apollo3.h`: Bit‑banged SPI via fastgpio (all pins usable). Provides `APOLLO3HardwareSPIOutput` with `writeBytes`, `writePixels`, and bit‑level toggling.
- `clockless_apollo3.h`: Clockless WS281x controller using SysTick for timing on Apollo3 Blue; sets `FASTLED_HAS_CLOCKLESS`.

## Supported boards and toolchains

Known‑good Arduino cores/boards:

- SparkFun Apollo3 (Artemis) core and boards: Edge, Thing Plus, ATP, Artemis Nano, LoRa Thing Plus (expLoRaBLE)
- Ambiq SDK‑based environments via the SparkFun core

Toolchain notes:

- Uses Ambiq's fastgpio for high‑rate toggling where available; otherwise falls back to standard GPIO which is significantly slower.
- Ensure your core exposes `am_hal_gpio_*` APIs used by `fastpin_apollo3.h`. When missing, hardware pin specialization will be disabled.

Timing caveats:

- `clockless_apollo3.h` relies on SysTick for delay loops; large interrupt windows can corrupt WS281x signaling. Prefer short ISRs during frame output.
- For best stability, keep Wi‑Fi/BLE stacks idle during `show()` and avoid heavy serial printing.
