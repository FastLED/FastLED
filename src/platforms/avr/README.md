# FastLED Platform: avr

AVR support for classic Arduino boards (e.g., ATmega328P/UNO) and many ATtiny/ATmega variants.

## Directory Structure

The AVR platform is organized by hardware capability to improve maintainability and reduce cognitive load:

```
platforms/avr/
  ├── attiny/              # ATtiny family (no MUL instruction, limited resources)
  │   ├── math/            # ATtiny-specific math implementations (software multiply)
  │   │   ├── math8_attiny.h
  │   │   └── scale8_attiny.h
  │   └── pins/            # ATtiny pin mappings (all ATtiny variants)
  │       └── fastpin_attiny.h
  │
  └── atmega/              # ATmega family (with MUL instruction)
      ├── common/          # Shared across all ATmega variants
      │   ├── avr_pin.h                        # Core _AVRPIN template
      │   ├── math8_avr.h                      # Optimized math (MUL instruction)
      │   ├── scale8_avr.h                     # Optimized scaling (AVR assembly)
      │   ├── trig8.h                          # Trigonometry functions
      │   ├── fastpin_avr_legacy_dispatcher.h  # Pin routing dispatcher
      │   └── fastpin_legacy_other.h           # Less common ATmega variants
      │
      ├── m328p/           # Arduino UNO, Nano (ATmega328P family)
      │   └── fastpin_m328p.h
      │
      ├── m32u4/           # Leonardo, Pro Micro (USB-capable ATmega32U4)
      │   └── fastpin_m32u4.h
      │
      ├── m2560/           # Arduino MEGA (high pin count)
      │   └── fastpin_m2560.h
      │
      └── m4809/           # Modern ATmega with VPORT registers
          ├── fastpin_avr_atmega4809.h
          └── fastpin_avr_nano_every.h
```

### Rationale: Hardware Capability Separation

The hierarchical structure reflects actual hardware differences:

1. **Instruction Set Differences**:
   - **ATtiny (classic)**: No MUL instruction → software multiplication required
   - **ATmega (all)**: MUL instruction → optimized assembly multiplication
   - Impact: Math/scale8 implementations route correctly via `LIB8_ATTINY` preprocessor define

2. **Register Architecture**:
   - **Legacy AVR**: DDR/PORT/PIN registers for GPIO
   - **Modern ATmega4809**: VPORT registers for faster GPIO
   - Impact: Pin manipulation templates differ significantly

3. **Shared AVR Code** (remains in `platforms/avr/` root):
   - Interrupt control, I/O abstractions, delay primitives
   - SPI backend infrastructure, clockless LED controllers
   - Millis timer implementations, platform entry points

### Backward Compatibility

All existing code continues to work unchanged. Dispatcher headers route to correct implementations transparently:
- `fastpin_avr.h` → dispatches to VPORT or legacy pin mappings
- `math8.h` → dispatches to ATtiny or ATmega math implementations
- `scale8.h` → dispatches to software or hardware multiply implementations

## What's here (quick pass)
- **avr_millis_timer_null_counter.hpp**: Defines a weak (or strong on some ATtiny) `volatile unsigned long timer_millis` symbol for boards missing a timer implementation.
- **avr_millis_timer0_impl_source.hpp**: Minimal Timer0/TCA0 ISR-based `timer_millis` implementation for select ATtinyx/y parts; initialized via constructor.
- **avr_millis_timer_source.cpp**: Chooses between weak symbol vs. implementation based on macros/board, includes the two headers above.
- **clockless_avr.h**: AVR clockless LED controller with tight-timing bit-banging (WS2811/WS2812). Docs on interrupt impact and timing macros.
- **compile_test.hpp**: Compile-time checks for AVR config (e.g., `FASTLED_USE_PROGMEM==1`, `FASTLED_ALLOW_INTERRUPTS==0`, `F_CPU` sanity).
- **fastled_avr.h**: Aggregator header enabling PROGMEM and including `fastpin_avr.h`, `fastspi_avr.h`, `clockless_avr.h`.
- **fastpin_avr.h**: Top-level dispatcher that routes to VPORT (ATmega4809/Nano Every) or legacy DDR/PORT/PIN pin mappings (all other AVRs).
- **fastpin_avr_legacy.h**: Backward-compatibility shim that forwards to `atmega/common/fastpin_avr_legacy_dispatcher.h` (deprecated, will be removed in v4.0).
- **math8.h**: Math dispatcher that routes to `attiny/math/math8_attiny.h` (software multiply) or `atmega/common/math8_avr.h` (hardware MUL) based on `LIB8_ATTINY`.
- **scale8.h**: Cross-platform scale dispatcher. On AVR, routes to `attiny/math/scale8_attiny.h` or `atmega/common/scale8_avr.h` based on `LIB8_ATTINY`.
- **trig8.h**: Cross-platform trig dispatcher. On AVR, routes to `atmega/common/trig8.h` (ATmega only, requires MUL instruction).
- **atmega/common/avr_pin.h**: Core `_AVRPIN` template used by `FastPin<>` specializations for direct port IO (hi/lo/toggle, masks, ports).
- **atmega/common/fastpin_avr_legacy_dispatcher.h**: Legacy pin dispatcher that routes to family-specific pin mappings (m328p, m32u4, m2560, attiny, or other).
- **atmega/m328p/fastpin_m328p.h**: Pin mappings for Arduino UNO, Nano, Pro Mini (ATmega328P/168P/8 family).
- **atmega/m32u4/fastpin_m32u4.h**: Pin mappings for Arduino Leonardo, Pro Micro, Teensy 2.0 (ATmega32U4, USB-capable).
- **atmega/m2560/fastpin_m2560.h**: Pin mappings for Arduino MEGA 2560 (ATmega2560/1280, high pin count).
- **atmega/m4809/fastpin_avr_atmega4809.h**: Pin mappings using VPORT for ATmega4809 parts; defines `AVR_HARDWARE_SPI`, `HAS_HARDWARE_PIN_SUPPORT`, SPI pin IDs.
- **atmega/m4809/fastpin_avr_nano_every.h**: Pin mappings using VPORT for Arduino Nano Every (ATmega4809 variant); hardware SPI constants.
- **atmega/common/fastpin_legacy_other.h**: Pin mappings for less common ATmega variants (ATmega644P, AT90USB, ATmega128RFA1, etc.).
- **attiny/pins/fastpin_attiny.h**: Pin mappings for all ATtiny variants (classic 25/45/85 and modern tinyAVR 0/1/2-series).
- **attiny/math/math8_attiny.h**: Software multiply implementation for ATtiny (no MUL instruction).
- **attiny/math/scale8_attiny.h**: Software scaling implementation for ATtiny (no MUL instruction).
- **atmega/common/math8_avr.h**: Optimized math implementation for ATmega (uses hardware MUL instruction).
- **atmega/common/scale8_avr.h**: Optimized scaling implementation for ATmega (uses hardware MUL instruction and AVR assembly).
- **atmega/common/trig8.h**: Trigonometry functions for ATmega (requires hardware MUL instruction).
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

Covered across the hierarchical structure:

### ATmega Family (with MUL instruction)
- **ATmega328P family** (`atmega/m328p/`): Arduino UNO, Nano, Pro Mini, ATmega328P/PB/168P/8
- **ATmega32U4 family** (`atmega/m32u4/`): Arduino Leonardo, Pro Micro, Teensy 2.0
- **ATmega2560 family** (`atmega/m2560/`): Arduino MEGA 2560, ATmega1280/2560
- **ATmega4809 family** (`atmega/m4809/`): Arduino Nano Every (VPORT-based GPIO)
- **Other ATmega variants** (`atmega/common/`): ATmega644P, ATmega1284P, AT90USB series, ATmega128RFA1, etc.

### ATtiny Family (no MUL instruction)
- **Classic ATtiny** (`attiny/`): ATtiny25/45/85, ATtiny24/44/84, ATtiny13/13A, ATtiny4313/2313, ATtiny88
- **Modern tinyAVR** (`attiny/`): 0/1/2-series (ATtiny202-417, ATtiny1604-1616, ATtinyxy2/xy4/xy6/xy7)

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
