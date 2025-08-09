# FastLED Platform: avr

AVR support for classic Arduino boards (e.g., ATmega328P/UNO) and many ATtiny/ATmega variants.

## What’s here (quick pass)
- **avr_compile.hpp**: Hierarchical include; ties AVR sources together when `FASTLED_ALL_SRC` is enabled (CMake globs bring `.cpp` in).
- **avr_millis_timer_null_counter.hpp**: Defines a weak (or strong on some ATtiny) `volatile unsigned long timer_millis` symbol for boards missing a timer implementation.
- **avr_millis_timer0_impl_source.hpp**: Minimal Timer0/TCA0 ISR-based `timer_millis` implementation for select ATtinyx/y parts; initialized via constructor.
- **avr_millis_timer_source.cpp**: Chooses between weak symbol vs. implementation based on macros/board, includes the two headers above.
- **avr_pin.h**: Core `_AVRPIN` template used by `FastPin<>` specializations for direct port IO (hi/lo/toggle, masks, ports).
- **clockless_trinket.h**: AVR clockless LED controller with tight-timing bit-banging (WS2811/WS2812). Docs on interrupt impact and timing macros.
- **compile_test.hpp**: Compile-time checks for AVR config (e.g., `FASTLED_USE_PROGMEM==1`, `FASTLED_ALLOW_INTERRUPTS==0`, `F_CPU` sanity).
- **fastled_avr.h**: Aggregator header enabling PROGMEM and including `fastpin_avr.h`, `fastspi_avr.h`, `clockless_trinket.h`.
- **fastpin_avr.h**: Dispatcher that includes either `fastpin_avr_nano_every.h` or `fastpin_avr_legacy.h` (ATmega4809/Nano Every vs. other AVRs).
- **fastpin_avr_atmega4809.h**: Pin mappings using VPORT for ATmega4809 parts; defines `AVR_HARDWARE_SPI`, `HAS_HARDWARE_PIN_SUPPORT`, SPI pin IDs.
- **fastpin_avr_nano_every.h**: Pin mappings using VPORT for Arduino Nano Every (ATmega4809 variant); hardware SPI constants.
- **fastpin_avr_legacy.h**: Large legacy “god-header” of pin mappings for many ATtiny/ATmega families; sets `HAS_HARDWARE_PIN_SUPPORT` and SPI aliases where applicable.
- **fastspi_avr.h**: SPI backends for AVR: UART MSPI (USART0/1) and hardware SPI (SPI/SPDR, SPI0) variants; `writeBytes`/`writePixels` helpers.
- **int.h**: AVR-friendly integer typedefs in `fl::` (e.g., `i16`, `u16`, `i32`, `u32`) and pointer/size aliases.
- **io_avr_lowlevel.h**: Low-level UART register helpers (UDR/UCSR mapping) plus `avr_uart_putchar/available/read` utilities.
- **io_avr.h**: Simple print/println and input helpers that use low-level UART if available, else fallback to Arduino `Serial`.
- **led_sysdefs_avr.h**: AVR platform defines (`FASTLED_AVR`), interrupt policy defaults, PROGMEM default, `MS_COUNTER` binding for millis symbols, small-device flags.

## Millis provider selection

- Boards with a working Arduino core `millis()` typically use the weak `timer_millis` symbol from `avr_millis_timer_null_counter.hpp` (no extra ISR).
- Select ATtiny parts (e.g., some x/y families) use `avr_millis_timer0_impl_source.hpp` to provide a minimal Timer0/TCA0 ISR that increments `timer_millis`.
- `avr_millis_timer_source.cpp` chooses between the two at compile time based on board macros.

## Supported families (summary)

Covered in `fastpin_avr_legacy.h` and `fastpin_avr_atmega4809.h`:

- ATmega328P/32U4/1280/2560 series
- ATmega4809 (Arduino Nano Every) via VPORT mappings
- ATtiny85/84/861 and newer `tinyAVR` 0/1‑series variants (where supported)

Quirks:

- Strict ISR timing: for WS281x, set `FASTLED_ALLOW_INTERRUPTS=0` on small AVRs for best signal integrity.
- SPI routes differ (SPI vs SPI0, UART MSPI). The SPI backend handles these variants; verify MOSI/SCK pins per board.

## Optional feature defines

- **`FASTLED_USE_PROGMEM`**: Default `1` on AVR. Required for flash‑resident tables.
- **`FASTLED_ALLOW_INTERRUPTS`**: Default `0` on AVR. You may set to `1` on faster parts but expect tighter timing constraints.
- **`FASTLED_FORCE_SOFTWARE_SPI`**: Force software (bit‑bang) SPI even when hardware SPI is available (commented hint in `fastspi_avr.h`).
- **`FASTLED_DEFINE_AVR_MILLIS_TIMER0_IMPL`**: Force Timer0 millis provider implementation for boards lacking Arduino `millis()`.
- **`FASTLED_DEFINE_TIMER_WEAK_SYMBOL`**: Control weak symbol export for `timer_millis` in `avr_millis_timer_source.cpp`.

Define these before including `FastLED.h`.
