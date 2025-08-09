## FastLED Platforms Directory

Table of Contents
- [What lives here](#what-lives-here)
- [Popular boards → where to look](#popular-boards--where-to-look)
- [Controller types in FastLED](#controller-types-in-fastled)
  - [Clockless (one-wire NRZ)](#1-clockless-onewire-nrz)
  - [SPI (clocked)](#2-spi-clocked)
- [Picking the right path](#picking-the-right-path)
- [Quick start examples](#quick-start-examples)
- [Backend selection cheat sheet](#backend-selection-cheat-sheet)
- [Troubleshooting and tips](#troubleshooting-and-tips)

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
 esp/32 I2S docs
```

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
