## FastLED Platforms Directory

Table of Contents
- [What lives here](#what-lives-here)
- [Popular boards → where to look](#popular-boards--where-to-look)
- [Controller types in FastLED](#controller-types-in-fastled)
  - [Clockless (one-wire NRZ)](#1-clockless-onewire-nrz)
  - [SPI (clocked)](#2-spi-clocked)
- [Picking the right path](#picking-the-right-path)
- [Backend selection cheat sheet](#backend-selection-cheat-sheet)
- [Troubleshooting and tips](#troubleshooting-and-tips)
- [Optional feature defines by platform](#optional-feature-defines-by-platform)
- [How to implement a clockless controller](#how-to-implement-a-clockless-controller)
  - [T1/T2/T3 vs T0H/T0L/T1H/T1L](#t1t2t3-vs-t0ht0lt1ht1l)
  - [Multilane output](#multilane-output)
- [How to implement a SPI controller](#how-to-implement-a-spi-controller)
  - [Hardware SPI vs software SPI](#hardware-spi-vs-software-spi)
  - [Common SPI LED chipsets](#common-spi-led-chipsets)
- [PROGMEM](#progmem)

The `src/platforms/` directory contains platform backends and integrations that FastLED targets. Each subfolder provides pin helpers, timing implementations, and controllers tailored to specific microcontrollers or host environments.

### What lives here

- [adafruit](./adafruit/README.md) – Adapter that lets you use Adafruit_NeoPixel via the FastLED API
- [apollo3](./apollo3/README.md) – Ambiq Apollo3 (SparkFun Artemis family)
- [arm](./arm/README.md) – ARM families (Teensy, SAMD21/SAMD51, RP2040, STM32/GIGA, nRF5x, Renesas UNO R4)
- [avr](./avr/README.md) – AVR (Arduino UNO, Nano Every, ATtiny/ATmega variants)
- [esp](./esp/README.md) – ESP8266 and ESP32 families
- [neopixelbus](./neopixelbus/README.md) – Adapter to drive NeoPixelBus through the FastLED API
- [posix](./posix/README.md) – POSIX networking helpers for host builds
- [shared](./shared/README.md) – Platform‑agnostic components ([ActiveStripData](./shared/README.md), [JSON UI](./shared/ui/json/readme.md))
- [stub](./stub/README.md) – Native, no‑hardware target used by unit tests/host builds
- [wasm](./wasm/README.md) – WebAssembly/browser target and JS bridges
- [win](./win/README.md) – Windows‑specific networking helpers

### Popular boards → where to look

- Arduino UNO (ATmega328P) → [avr](./avr/README.md)
- Arduino Nano Every (ATmega4809) → [avr](./avr/README.md)
- ATtiny85/other ATtiny (supported subsets) → [avr](./avr/README.md)
- Teensy 3.x (MK20) → [arm/k20](./arm/k20/README.md)
- Teensy 3.6 (K66) → [arm/k66](./arm/k66/README.md)
- Teensy LC (KL26) → [arm/kl26](./arm/kl26/README.md)
- Teensy 4.x (i.MX RT1062) → [arm/mxrt1062](./arm/mxrt1062/README.md)
- Arduino Zero / SAMD21 → [arm/d21](./arm/d21/README.md)
- Feather/Itsy M4, Wio Terminal / SAMD51 → [arm/d51](./arm/d51/README.md)
- Arduino GIGA R1 (STM32H747) → [arm/giga](./arm/giga/README.md)
- STM32 (e.g., F1 series) → [arm/stm32](./arm/stm32/README.md)
- Raspberry Pi Pico (RP2040) → [arm/rp2040](./arm/rp2040/README.md)
- Nordic nRF51/nRF52 → [arm/nrf51](./arm/nrf51/README.md), [arm/nrf52](./arm/nrf52/README.md)
- Arduino UNO R4 (Renesas RA4M1) → [arm/renesas](./arm/renesas/README.md)
- ESP8266 → [esp/8266](./esp/8266/README.md)
- ESP32 family (ESP32, S2, S3, C3, C6) → [esp/32](./esp/32/README.md)
- Ambiq Apollo3 (Artemis) → [apollo3](./apollo3/README.md)

If you are targeting a desktop/browser host for demos or tests:
- Native host tests → [stub](./stub/README.md) (with [shared](./shared/README.md) for data/UI)
- Browser/WASM builds → [wasm](./wasm/README.md)

### Controller types in FastLED

FastLED provides two primary controller categories. Choose based on your LED chipset and platform capabilities.

#### 1) Clockless (one‑wire NRZ)

- For WS2811/WS2812/WS2812B/SK6812 and similar “NeoPixel”‑class LEDs
- Encodes bits as precisely timed high/low pulses (T0H/T1H/T0L/T1L)
- Implemented with tight CPU timing loops, SysTick, or hardware assist (e.g., [ESP32 RMT or I2S‑parallel](./esp/32/README.md))
- Available as single‑lane and, on some platforms, multi‑lane “block clockless” drivers
- Sensitive to long interrupts; keep ISRs short during `show()`

Special cases:
- ESP32 offers multiple backends for clockless output (see [esp/32](./esp/32/README.md)):
  - RMT (recommended, per‑channel timing)
  - I2S‑parallel (many strips with identical timing)
  - Clockless over SPI (WS2812‑class only)

#### 2) SPI (clocked)

- For chipsets with data+clock lines (APA102/DotStar, SK9822, LPD8806, WS2801)
- Uses hardware SPI peripherals to shift pixels with explicit clocking
- Generally tolerant of interrupts and scales to higher data rates more easily
- Requires an SPI‑capable LED chipset (not for WS2812‑class, except the ESP32 “clockless over SPI” path above)

### Picking the right path

- WS2812/SK6812 → Clockless controllers (ESP32: RMT or I2S‑parallel for many identical strips)
- APA102/SK9822/DotStar → SPI controllers for stability and speed
- Prefer [neopixelbus](./neopixelbus/README.md) or [adafruit](./adafruit/README.md) adapters if you rely on those ecosystems but want to keep FastLED’s API
RP2040 (PIO clockless) needs no special defines; use standard WS2812 addLeds call.

### Backend selection cheat sheet

- [ESP32](./esp/32/README.md) clockless: RMT (flexible), I2S‑parallel (many identical strips), or SPI‑WS2812 path (WS2812‑only)
- Teensy 3.x/4.x: clockless (single or block multi‑lane), plus OctoWS2811/SmartMatrix integrations (see [arm/k20](./arm/k20/README.md), [arm/mxrt1062](./arm/mxrt1062/README.md))
- [AVR](./avr/README.md): clockless WS2812 and SPI chipsets; small devices benefit from fewer interrupts during `show()`

### Troubleshooting and tips

- First‑pixel flicker or color glitches often indicate interrupt interference. Reduce background ISRs (Wi‑Fi/BLE/logging) during `FastLED.show()` or select a hardware‑assisted backend.
- Power and level shifting matter: WS281x typically need 5V data at sufficient current; ensure a common ground between controller and LEDs.
- On ARM families, PROGMEM is typically disabled (`FASTLED_USE_PROGMEM=0`); on AVR it is enabled.
- For very large installations, consider parallel outputs (OctoWS2811 on Teensy, I2S‑parallel on ESP32) and DMA‑friendly patterns.

### How to implement a clockless controller

Clockless controllers generate the one‑wire NRZ waveforms used by WS281x‑class LEDs by toggling a GPIO with precise timings. FastLED’s clockless base controllers accept three timing parameters per bit: T1, T2, T3.

#### T1/T2/T3 vs T0H/T0L/T1H/T1L

The timing model is:

- At T=0: line goes high to start a bit
- At T=T1: drop low to transmit a 0‑bit (T0H just ended)
- At T=T1+T2: drop low to transmit a 1‑bit (T1H just ended)
- At T=T1+T2+T3: bit cycle ends; next bit can start

Mapping to datasheet values:

- T0H = T1
- T1H = T2
- T0L = duration − T0H
- T1L = duration − T1H

Where duration is the max of the two bit periods: `max(T0H + T0L, T1H + T1L)`.

Embedded helper script (from `src/chipsets.h`) to derive T1/T2/T3 from datasheet T0H/T0L/T1H/T1L:

```python
print("Enter the values of T0H, T0L, T1H, T1L, in nanoseconds: ")
T0H = int(input("  T0H: "))
T0L = int(input("  T0L: "))
T1H = int(input("  T1H: "))
T1L = int(input("  T1L: "))

duration = max(T0H + T0L, T1H + T1L)

print("The max duration of the signal is: ", duration)

T1 = T0H
T2 = T1H
T3 = duration - T0H - T0L

print("T1: ", T1)
print("T2: ", T2)
print("T3: ", T3)
```

Notes:
- Some platforms express timings in CPU cycles rather than nanoseconds; FastLED provides converters/macros to derive cycle counts.
- See the platform‑specific `ClocklessController` implementation for the required units and conversion helpers.

#### Multilane output

“Multilane” or “block clockless” controllers send several strips in parallel by transposing pixel data into per‑bit planes and toggling multiple pins simultaneously.

- Benefits: High aggregate throughput; useful for matrices and large installations.
- Constraints: All lanes must share the same timing and frame cadence; ISR windows apply across all lanes during output.

Supported in this codebase:

- Teensy 3.x (K20): [clockless_block_arm_k20.h](./arm/k20/README.md)
- Teensy 3.6 (K66): [clockless_block_arm_k66.h](./arm/k66/README.md)
- Teensy 4.x (i.MX RT1062): [block_clockless_arm_mxrt1062.h](./arm/mxrt1062/README.md)
- Arduino Due (SAM3X): [clockless_block_arm_sam.h](./arm/sam/README.md)
- ESP8266: [clockless_block_esp8266.h](./esp/8266/README.md)
- ESP32: I2S‑parallel backend (see [esp/32](./esp/32/README.md)) provides many‑lane output with identical timing

Typically single‑lane only in this codebase:

- AVR family
- RP2040 (PIO driver here is single‑lane)
- nRF51/nRF52
- STM32/GIGA (clockless single‑lane in this tree)

For Teensy, also consider OctoWS2811 for DMA‑driven parallel output.

### How to implement a SPI controller

SPI‑driven LEDs use a clocked data line plus a clock line. Implementation is generally simpler and more ISR‑tolerant than clockless.

#### Hardware SPI vs software SPI

- Hardware SPI: Uses the MCU’s SPI peripheral; best performance and CPU efficiency; works with DMA on some platforms.
- Software SPI (bit‑bang): Uses GPIO toggling when a hardware SPI peripheral or pins aren’t available; slower and more CPU‑intensive but universally portable.

FastLED’s SPIOutput abstracts both; platform headers select efficient implementations where available.

#### Common SPI LED chipsets

- APA102 / “DotStar” (and HD variants): Data + clock + per‑pixel global brightness; tolerant of interrupts. See [APA102 controller](../chipsets.h) definitions.
- SK9822: Protocol compatible with APA102 with minor differences; supported.
- WS2801, LPD8806, LPD6803, P9813, SM16716: Older or niche SPI‑style protocols; supported via specific controllers.

When choosing SPI speed, consult chipset and wiring limits (signal integrity on long runs). Long APA102 runs may require reduced clock rates.

### PROGMEM

Some platforms store constant tables and palettes in flash (program) memory rather than RAM. FastLED abstracts PROGMEM usage via macros; support varies by platform.

- AVR: Full PROGMEM support; default `FASTLED_USE_PROGMEM=1`. Flash is separate from RAM; use `pgm_read_` accessors for reads.
- ESP8266: PROGMEM supported; see [progmem_esp8266.h](./esp/8266/progmem_esp8266.h). Provides `FL_PROGMEM`, `FL_PGM_READ_*`, and `FL_ALIGN_PROGMEM` (4‑byte alignment) helpers.
- ESP32, ARM (Teensy, SAMD21/51, RP2040, STM32/GIGA, nRF): Typically treat constants as directly addressable; FastLED defaults to `FASTLED_USE_PROGMEM=0` on these families to avoid unnecessary indirection.

Aligned reads for PROGMEM:

- On platforms that require alignment (e.g., ESP8266), use 4‑byte alignment for multibyte table entries to avoid unaligned access traps. FastLED exposes `FL_ALIGN_PROGMEM` for this purpose.

Platforms without PROGMEM (or where it’s a no‑op in this codebase):

- Most ARM families (SAMD, STM32, nRF, RP2040) and ESP32 use memory‑mapped flash; PROGMEM macros are disabled here via `FASTLED_USE_PROGMEM=0`.

Check each platform’s `led_sysdefs_*` header for the recommended PROGMEM and interrupt policy.

### Optional feature defines by platform

Below is a non-exhaustive list of optional or tunable feature-defines, grouped by platform. Define these before including `FastLED.h` in your sketch.

- ESP32 (`esp/32`)
  - General: `FASTLED_USE_PROGMEM` (default 0), `FASTLED_ALLOW_INTERRUPTS` (default 1), `FASTLED_ESP32_RAW_PIN_ORDER`
  - Backend selection: `FASTLED_ESP32_I2S`, `FASTLED_ESP32_USE_CLOCKLESS_SPI` (WS2812-only). RMT is selected by default when available
  - SPI: `FASTLED_ALL_PINS_HARDWARE_SPI`, `FASTLED_ESP32_SPI_BUS` (VSPI/HSPI/FSPI), `FASTLED_ESP32_SPI_BULK_TRANSFER` (0/1), `FASTLED_ESP32_SPI_BULK_TRANSFER_SIZE`
  - RMT (IDF4): `FASTLED_RMT_BUILTIN_DRIVER`, `FASTLED_RMT_SERIAL_DEBUG`, `FASTLED_RMT_MEM_WORDS_PER_CHANNEL`, `FASTLED_RMT_MEM_BLOCKS`, `FASTLED_RMT_MAX_CONTROLLERS`, `FASTLED_RMT_MAX_CHANNELS`, `FASTLED_RMT_MAX_TICKS_FOR_GTX_SEM`, `FASTLED_ESP32_FLASH_LOCK`
  - RMT (IDF5): uses DMA internally (`FASTLED_RMT_USE_DMA` marker)
  - I2S: `I2S_DEVICE`, `FASTLED_I2S_MAX_CONTROLLERS`, `FASTLED_ESP32_I2S_NUM_DMA_BUFFERS`
  - Logging/size: `FASTLED_ESP32_ENABLE_LOGGING`, `FASTLED_ESP32_MINIMAL_ERROR_HANDLING`
  - Clock override: `F_CPU_RMT_CLOCK_MANUALLY_DEFINED`
  - Misc: `FASTLED_INTERRUPT_RETRY_COUNT`, `FASTLED_DEBUG_COUNT_FRAME_RETRIES`

- ESP8266 (`esp/8266`)
  - General: `FASTLED_USE_PROGMEM` (default 0), `FASTLED_ALLOW_INTERRUPTS` (default 1)
  - Pin order: `FASTLED_ESP8266_RAW_PIN_ORDER`, `FASTLED_ESP8266_NODEMCU_PIN_ORDER`, `FASTLED_ESP8266_D1_PIN_ORDER`
  - SPI: `FASTLED_ALL_PINS_HARDWARE_SPI`
  - Debug: `FASTLED_DEBUG_COUNT_FRAME_RETRIES`
  - Global: `FASTLED_INTERRUPT_RETRY_COUNT`

- AVR (`avr`)
  - `FASTLED_USE_PROGMEM` (default 1), `FASTLED_ALLOW_INTERRUPTS` (default 0), `FASTLED_FORCE_SOFTWARE_SPI`
  - Millis provider: `FASTLED_DEFINE_AVR_MILLIS_TIMER0_IMPL`, `FASTLED_DEFINE_TIMER_WEAK_SYMBOL`

- ARM SAMD21 (`arm/d21`) and SAMD51 (`arm/d51`)
  - `FASTLED_USE_PROGMEM` (default 0), `FASTLED_ALLOW_INTERRUPTS` (default 1, enables `FASTLED_ACCURATE_CLOCK`)

- STM32/GIGA (`arm/stm32`, `arm/giga`)
  - `FASTLED_USE_PROGMEM` (default 0), `FASTLED_ALLOW_INTERRUPTS` (STM32 default 0), `FASTLED_ACCURATE_CLOCK` (when interrupts on), `FASTLED_NO_PINMAP`, `FASTLED_NEEDS_YIELD`

- Teensy 3.x K20/K66 (`arm/k20`, `arm/k66`)
  - `FASTLED_USE_PROGMEM` (often 1), `FASTLED_ALLOW_INTERRUPTS` (1), `FASTLED_ACCURATE_CLOCK`
  - ObjectFLED: `FASTLED_OBJECTFLED_LATCH_DELAY`

- Teensy LC KL26 (`arm/kl26`)
  - `FASTLED_USE_PROGMEM` (1), `FASTLED_ALLOW_INTERRUPTS` (1), `FASTLED_SPI_BYTE_ONLY`

- RP2040 (`arm/rp2040`)
  - `FASTLED_USE_PROGMEM` (0), `FASTLED_ALLOW_INTERRUPTS` (1), `FASTLED_ACCURATE_CLOCK`
  - Clockless selection: `FASTLED_RP2040_CLOCKLESS_PIO` (1), `FASTLED_RP2040_CLOCKLESS_IRQ_SHARED` (1), `FASTLED_RP2040_CLOCKLESS_M0_FALLBACK` (0)

- nRF51 (`arm/nrf51`)
  - `FASTLED_USE_PROGMEM` (0), `FASTLED_ALLOW_INTERRUPTS` (1), `FASTLED_ALL_PINS_HARDWARE_SPI`

- nRF52 (`arm/nrf52`)
  - `FASTLED_USE_PROGMEM` (0), `FASTLED_ALLOW_INTERRUPTS` (1)
  - SPI/clockless options: `FASTLED_ALL_PINS_HARDWARE_SPI`, `FASTLED_NRF52_SPIM`, `FASTLED_NRF52_ENABLE_PWM_INSTANCE0`, `FASTLED_NRF52_NEVER_INLINE`, `FASTLED_NRF52_MAXIMUM_PIXELS_PER_STRING`, `FASTLED_NRF52_PWM_ID`, `FASTLED_NRF52_SUPPRESS_UNTESTED_BOARD_WARNING`

- Renesas UNO R4 (`arm/renesas`)
  - `FASTLED_USE_PROGMEM` (0), `FASTLED_ALLOW_INTERRUPTS` (1), `FASTLED_NO_PINMAP`

- Adafruit adapter (`adafruit`)
  - `FASTLED_USE_ADAFRUIT_NEOPIXEL` (0/1)

- NeoPixelBus adapter (`neopixelbus`)
  - `FASTLED_USE_NEOPIXEL_BUS` (0/1)

- WASM (`wasm`)
  - `FASTLED_STUB_IMPL`, `FASTLED_HAS_MILLIS` (1), `FASTLED_USE_PROGMEM` (0), `FASTLED_ALLOW_INTERRUPTS` (1)

- Stub/native tests (`stub`)
  - `FASTLED_STUB_IMPL`, `FASTLED_HAS_MILLIS` (1), `FASTLED_ALLOW_INTERRUPTS` (1), `FASTLED_USE_PROGMEM` (0)
  - Stub helpers: `FASTLED_USE_PTHREAD_DELAY`, `FASTLED_USE_PTHREAD_YIELD`
