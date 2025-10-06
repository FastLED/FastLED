# FastLED - The Universal LED Library

**Transform any microcontroller into an LED powerhouse.**

Drive **30,000+ LEDs** on high-end devices • **Sub-$1 compatibility** on tiny chips • **Background rendering** for responsive apps • **Nearly every LED chipset supported** • [**#2 most popular Arduino library**](https://docs.arduino.cc/libraries/)

[![Arduino's 2nd Most Popular Library](https://www.ardu-badge.com/badge/FastLED.svg)](https://www.ardu-badge.com/FastLED) [![Build Status](https://github.com/FastLED/FastLED/workflows/build/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build.yml) [![Unit Tests](https://github.com/FastLED/FastLED/actions/workflows/build_unit_test.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_unit_test.yml) [![Documentation](https://img.shields.io/badge/Docs-Doxygen-blue.svg)](http://fastled.io/docs) [![Community](https://img.shields.io/badge/reddit-/r/FastLED-orange.svg?logo=reddit)](https://www.reddit.com/r/FastLED/)


## ⚡ Get Blinking in 30 Seconds

```cpp
#include <FastLED.h>
#define NUM_LEDS 60
CRGB leds[NUM_LEDS];

void setup() { 
  FastLED.addLeds<WS2812, 6>(leds, NUM_LEDS); 
}

void loop() {
  leds[0] = CRGB::Red; FastLED.show(); delay(500);
  leds[0] = CRGB::Blue; FastLED.show(); delay(500);
}
```

**✅ Works on Arduino, ESP32, Teensy, Raspberry Pi, and 50+ other platforms**

## 🚀 Why FastLED?

| **Massive Scale** | **Tiny Footprint** | **Background Rendering** | **Universal** |
|-------------------|---------------------|--------------------------|---------------|
| Drive 30,000 LEDs on Teensy 4.1 | Runs on $0.50 ATtiny chips | ESP32/Teensy render while you code | Works on 50+ platforms |
| 50 parallel strips on Teensy | <2KB on Arduino Uno | Never miss user input | Nearly every LED chipset |

**🎯 Performance**: Zero-cost global brightness • High-performance 8-bit math, memory efficient on platforms that need it.
**🔧 Developer Experience**: Quick platform switching • Extensive examples • Active community support

## 📂 Source Code Directory Structure

FastLED's codebase is organized into several major areas. Each directory contains a README with detailed documentation:

| **Path** | **Description** |
|----------|-----------------|
| [src/README.md](src/README.md) | Overview of the FastLED source tree including public headers, core foundation, effects, platforms, sensors, and third-party integrations |
| [src/fl/README.md](src/fl/README.md) | FastLED core library (`fl::`) providing cross-platform STL-like containers, graphics primitives, math, async, JSON, and I/O utilities |
| [src/fx/README.md](src/fx/README.md) | FX library with 1D/2D effects, video playback, composition utilities, and links to effect subdirectories |
| [src/platforms/readme.md](src/platforms/readme.md) | Platform directory overview with backend selection guide, controller types, board-specific documentation, and links to all platform subdirectories |

## Table of Contents

- [🆕 Latest Feature](#-latest-feature)
- [⭐ Community Growth](#-community-growth)
- [🆕 Latest Features](#-latest-features)
- [🌍 Platform Support](#-platform-support)
- [📦 Installation](#-installation)
- [📚 Documentation & Support](#-documentation--support)
- [🎮 Advanced Features](#-advanced-features)
- [🤝 Contributing](#-contributing)

📊 <strong>Detailed Build Status</strong>

### Arduino Family
**Core Boards:** [![uno](https://github.com/FastLED/FastLED/actions/workflows/build_uno.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_uno.yml) [![nano_every](https://github.com/FastLED/FastLED/actions/workflows/build_nano_every.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_nano_every.yml) [![uno_r4_wifi](https://github.com/FastLED/FastLED/actions/workflows/build_uno_r4_wifif.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_uno_r4_wifif.yml) [![yun](https://github.com/FastLED/FastLED/actions/workflows/build_yun.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_yun.yml)

**ARM Boards:** [![due](https://github.com/FastLED/FastLED/actions/workflows/build_due.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_due.yml) [![digix](https://github.com/FastLED/FastLED/actions/workflows/build_digix.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_digix.yml) [![Arduino Giga-R1](https://github.com/FastLED/FastLED/actions/workflows/build_giga_r1.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_giga_r1.yml)*

**ATtiny Series:** [![attiny85](https://github.com/FastLED/FastLED/actions/workflows/build_attiny85.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_attiny85.yml) [![attiny88](https://github.com/FastLED/FastLED/actions/workflows/build_attiny88.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_attiny88.yml) [![attiny1604](https://github.com/FastLED/FastLED/actions/workflows/build_attiny1604.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_attiny1604.yml) [![attiny1616](https://github.com/FastLED/FastLED/actions/workflows/build_attiny1616.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_attiny1616.yml) [![attiny4313](https://github.com/FastLED/FastLED/actions/workflows/build_attiny4313.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_attiny4313.yml)**

*Notes: * Giga-R1 support added in 3.9.14 • ** ATtiny4313 has limited memory (WS2812 Blink + APA102 examples only)

### Teensy Series
**Standard Models:** [![teensy30](https://github.com/FastLED/FastLED/actions/workflows/build_teensy30.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_teensy30.yml) [![teensy31](https://github.com/FastLED/FastLED/actions/workflows/build_teensy31.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_teensy31.yml) [![teensyLC](https://github.com/FastLED/FastLED/actions/workflows/build_teensyLC.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_teensyLC.yml) [![teensy40](https://github.com/FastLED/FastLED/actions/workflows/build_teensy40.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_teensy40.yml) [![teensy41](https://github.com/FastLED/FastLED/actions/workflows/build_teensy41.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_teensy41.yml)

**Special Features:** [![teensy_octoWS2811](https://github.com/FastLED/FastLED/actions/workflows/build_teensy_octo.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_teensy_octo.yml) [![teensy41 ObjectFLED](https://github.com/FastLED/FastLED/actions/workflows/build_teensy41_ofled.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_teensy41_ofled.yml)

### NRF52 (Nordic)
[![nrf52840_sense](https://github.com/FastLED/FastLED/actions/workflows/build_adafruit_feather_nrf52840_sense.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_adafruit_feather_nrf52840_sense.yml) [![nordicnrf52_dk](https://github.com/FastLED/FastLED/actions/workflows/build_nrf52840_dk.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_nrf52840_dk.yml) [![adafruit_xiaoblesense](https://github.com/FastLED/FastLED/actions/workflows/build_adafruit_xiaoblesense.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_adafruit_xiaoblesense.yml) [![nrf52_xiaoblesense](https://github.com/FastLED/FastLED/actions/workflows/build_nrf52_xiaoblesense.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_nrf52_xiaoblesense.yml)

*Note: NRF52 XiaoBLE board has mbed engine but doesn't compile against Arduino.h for unknown reasons.

### Apollo3 (Ambiq)
[![apollo3_red](https://github.com/FastLED/FastLED/actions/workflows/build_apollo3_red.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_apollo3_red.yml) [![apollo3_thing_explorable](https://github.com/FastLED/FastLED/actions/workflows/build_apollo3_thing_explorable.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_apollo3_thing_explorable.yml)

*Beta support added in 3.10.2

### STM32 (STMicroelectronics)
[![Bluepill (STM32F103C8)](https://github.com/FastLED/FastLED/actions/workflows/build_bluepill.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_bluepill.yml) [![Blackpill (STM32F411CE)](https://github.com/FastLED/FastLED/actions/workflows/build_blackpill_stm32f4.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_blackpill_stm32f4.yml) [![Maple Mini (STM32F103CB)](https://github.com/FastLED/FastLED/actions/workflows/build_maple_map.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_maple_map.yml) [![stm103tb](https://github.com/FastLED/FastLED/actions/workflows/build_stm103tb.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_stm103tb.yml)

*Note: STM103TB has limited PlatformIO support

### Silicon Labs (SiLabs)
[![ThingPlusMatter_mgm240s](https://github.com/FastLED/FastLED/actions/workflows/build_mgm240s_thingplusmatter.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_mgm240s_thingplusmatter.yml)

*MGM240 (EFR32MG24) support for Arduino Nano Matter, SparkFun Thing Plus Matter, and Seeed Xiao MG24 Sense boards

### Raspberry Pi Pico
[![rp2040](https://github.com/FastLED/FastLED/actions/workflows/build_rp2040.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_rp2040.yml) [![rp2350](https://github.com/FastLED/FastLED/actions/workflows/build_rp2350.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_rp2350.yml) [![rp2350B SparkfunXRP](https://github.com/FastLED/FastLED/actions/workflows/build_rp2350B.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_rp2350B.yml)

### ESP32 (Espressif)
**ESP8266:** [![esp32-8266](https://github.com/FastLED/FastLED/actions/workflows/build_esp8622.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_esp8622.yml)

**ESP32 Classic:** [![esp32dev](https://github.com/FastLED/FastLED/actions/workflows/build_esp32dev.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_esp32dev.yml) [![esp32wroom](https://github.com/FastLED/FastLED/actions/workflows/build_esp32wroom.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_esp32wroom.yml)

**ESP32 S-Series:** [![esp32s2](https://github.com/FastLED/FastLED/actions/workflows/build_esp32s2.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_esp32s2.yml) [![esp32s3](https://github.com/FastLED/FastLED/actions/workflows/build_esp32s3.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_esp32s3.yml)

**ESP32 C-Series:** [![esp32c2](https://github.com/FastLED/FastLED/actions/workflows/build_esp32c2.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_esp32c2.yml)* [![esp32c3](https://github.com/FastLED/FastLED/actions/workflows/build_esp32c3.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_esp32c3.yml) [![esp32c5](https://github.com/FastLED/FastLED/actions/workflows/build_esp32c5.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_esp32c5.yml)[![esp32c6](https://github.com/FastLED/FastLED/actions/workflows/build_esp32c6.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_esp32c6.yml)

**ESP32 Advanced:** [![esp32h2](https://github.com/FastLED/FastLED/actions/workflows/build_esp32h2.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_esp32h2.yml) [![esp32p4](https://github.com/FastLED/FastLED/actions/workflows/build_esp32p4.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_esp32p4.yml)

**Special Features:** [![esp32_i2s_ws2812](https://github.com/FastLED/FastLED/actions/workflows/build_esp32_i2s_ws2812.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_esp32_i2s_ws2812.yml) [![esp32 extra libs](https://github.com/FastLED/FastLED/actions/workflows/build_esp_extra_libs.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_esp_extra_libs.yml) [![esp32dev_namespace](https://github.com/FastLED/FastLED/actions/workflows/build_esp32dev_namespace.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_esp32dev_namespace.yml)

**QEMU Tests:** [![ESP32-DEV QEMU Test](https://github.com/FastLED/FastLED/actions/workflows/qemu_esp32dev_test.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/qemu_esp32dev_test.yml) [![ESP32-C3 QEMU Test](https://github.com/FastLED/FastLED/actions/workflows/qemu_esp32c3_test.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/qemu_esp32c3_test.yml) [![ESP32-S3 QEMU Test](https://github.com/FastLED/FastLED/actions/workflows/qemu_esp32s3_test.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/qemu_esp32s3_test.yml) [![ESP32-P4 QEMU Test](https://github.com/FastLED/FastLED/actions/workflows/qemu_esp32p4_test.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/qemu_esp32p4_test.yml)

**Legacy:** [![esp32dev-idf3.3-lts](https://github.com/FastLED/FastLED/actions/workflows/build_esp32dev_idf3.3.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_esp32dev_idf3.3.yml)

*Notes: * ESP32-C2 support added in 3.9.10 • [Espressif compatibility evaluation](https://github.com/espressif/arduino-esp32/blob/gh-pages/LIBRARIES_TEST.md)

### Specialty Platforms
**x86:** [![linux_native](https://github.com/FastLED/FastLED/actions/workflows/build_linux.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_linux.yml)

**WebAssembly:** [![wasm](https://github.com/FastLED/FastLED/actions/workflows/build_wasm.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_wasm.yml) [![wasm_compile_test](https://github.com/FastLED/FastLED/actions/workflows/build_wasm_compilers.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_wasm_compilers.yml)

### Library Size Validation
**Core Platforms:** [![attiny85_binary_size](https://github.com/FastLED/FastLED/actions/workflows/check_attiny85.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/check_attiny85.yml) [![uno_binary_size](https://github.com/FastLED/FastLED/actions/workflows/check_uno_size.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/check_uno_size.yml) [![esp32dev_binary_size](https://github.com/FastLED/FastLED/actions/workflows/check_esp32_size.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/check_esp32_size.yml)

**Teensy Platforms:** [![check_teensylc_size](https://github.com/FastLED/FastLED/actions/workflows/check_teensylc_size.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/check_teensylc_size.yml) [![check_teensy30_size](https://github.com/FastLED/FastLED/actions/workflows/check_teensy30_size.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/check_teensy30_size.yml) [![check_teensy31_size](https://github.com/FastLED/FastLED/actions/workflows/check_teensy31_size.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/check_teensy31_size.yml) [![teensy41_binary_size](https://github.com/FastLED/FastLED/actions/workflows/check_teensy41_size.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/check_teensy41_size.yml)


## ⭐ Community Growth

<a href="https://star-history.com/#fastled/fastled&Date">
 <picture>
   <source media="(prefers-color-scheme: dark)" srcset="https://api.star-history.com/svg?repos=fastled/fastled&type=Date&theme=dark" />
   <source media="(prefers-color-scheme: light)" srcset="https://api.star-history.com/svg?repos=fastled/fastled&type=Date" />
   <img alt="Star History Chart" src="https://api.star-history.com/svg?repos=fastled/fastled&type=Date" />
 </picture>
</a>

## 🆕 Latest Features

## **FastLED 3.10.2: Corkscrew Mapping**

See [examples/FestivalStick/FestivalStick.ino](examples/FestivalStick/FestivalStick.ino)

https://github.com/user-attachments/assets/19af4b54-c189-482b-b13b-2d7f73efa4e0

### **FastLED 3.10.0: Animartrix Out of Beta**
Advanced animation framework for complex LED visualizations and effects.

[![FastLED Animartrix Demo](https://fastled.github.io/assets/fastled_3_10_animartrix.jpg)](https://fastled.github.io/assets/fastled_3_10_animartrix.mp4)

### **FastLED 3.9.16: WaveFx / Multi Layer Compositing**
Multi-layer compositing & time-based animation control for tech-artists.


https://github.com/user-attachments/assets/ff8e0432-3e0d-47cc-a444-82ce27f562af

| **3.9.13** | **3.9.10** | **3.9.8** | **3.9.2** |
|------------|------------|-----------|-----------|
| [**HD107 Turbo**](#new-in-3913-hd107-turbo-40mhz-led-support)<br/>40MHz LED support | [**ESP32 SPI**](#new-in-3910-super-stable-ws2812-spi-driver-for-esp32)<br/>Super stable WS2812 driver | [**Massive Teensy**](#new-in-398---massive-teensy-41--40-ws2812-led-output)<br/>50 parallel pins on 4.1 | [**WS2812 Overclock**](#new-in-392---overclocking-of-ws2812)<br/>Up to 70% speed boost |

| **3.7.7** | **More Features** |
|-----------|-------------------|
| [**RGBW Support**](#new-in-377---rgbw-led-strip-support)<br/>White channel LED strips | [📋 Full Changelog](https://github.com/FastLED/FastLED/releases)<br/>[📺 Demo Videos](https://zackees.github.io/fastled-wasm/) |

[**📺 Live Demos**](https://zackees.github.io/fastled-wasm/) • [**📋 Full Changelog**](https://github.com/FastLED/FastLED/releases)

<details>
<summary>📖 <strong>Detailed Feature Information</strong> (Click to expand)</summary>

## New in 3.10.2: Corkscrew Mapping

See [examples/FestivalStick/FestivalStick.ino](examples/FestivalStick/FestivalStick.ino)

https://github.com/user-attachments/assets/19af4b54-c189-482b-b13b-2d7f73efa4e0

## New in 3.10.0: Animartrix out of beta

[![FastLED 3.10 Animartrix](https://fastled.github.io/assets/fastled_3_10_animartrix.jpg)](https://fastled.github.io/assets/fastled_3_10_animartrix.mp4)


## New in 3.9.16: WaveFx / Multi Layer Compositing / Time-based animation control

Video:



https://github.com/user-attachments/assets/ff8e0432-3e0d-47cc-a444-82ce27f562af



#### Major release for tech-artists!

Lots of improvements in this release, read the full [change list here](https://github.com/FastLED/FastLED/releases/tag/3.9.16)

#### Links

  * This demo -> [FxWave2d](https://github.com/FastLED/FastLED/blob/master/examples/FxWave2d/FxWave2d.ino)
    * [Wave Simulation Library](https://github.com/FastLED/FastLED/blob/master/src/fl/wave_simulation.h)
  * [FireCylinder](https://github.com/FastLED/FastLED/blob/master/examples/FireCylinder/FireCylinder.ino)
    * Wraps around so that (0,y) ~= (width-1,y)
  * [TimeAlpha](https://github.com/FastLED/FastLED/blob/master/src/fl/time_alpha.h)
    * Precision control of animations with time-based alpha transition.



## New in 3.9.13: HD107 "Turbo" 40Mhz LED Support

![image](https://github.com/user-attachments/assets/9684ab7d-2eaa-40df-a00d-0dff18098917)

## New in 3.9.12: WS2816 "HD" LED support

![image](https://github.com/user-attachments/assets/258ec44c-af82-44b7-ad7b-fac08daa9bcb)

## New in 3.9.10: Super Stable WS2812 SPI driver for ESP32

![image (2)](https://github.com/user-attachments/assets/b3c5801c-66df-40af-a6b8-bbd1520fbb36)

## New in 3.9.9: 16-way Yves I2S parallel driver for the ESP32-S3

![perpetualmaniac_an_led_display_in_a_room_lots_of_refaction_of_t_eb7c170a-7b2c-404a-b114-d33794b4954b](https://github.com/user-attachments/assets/982571fc-9b8d-4e58-93be-5bed76a0c53d)

*Note some users find that newer versions of the ESP32 Arduino core (3.10) don't work very well, but older versions do, see [issue 1903](https://github.com/FastLED/FastLED/issues/1903)

## New in 3.9.8 - Massive Teensy 4.1 & 4.0 WS2812 LED output

  * *Teensy 4.1: 50 parallel pins*
  * *Teensy 4.0: 42 parallel pins*

![New Project](https://github.com/user-attachments/assets/79dc2801-5161-4d5a-90a2-0126403e215f)




## New in 3.9.2 - Overclocking of WS2812
![image](https://github.com/user-attachments/assets/be98fbe6-0ec7-492d-8ed1-b7eb6c627e86)
Update: max overclock has been reported at +70%: https://www.reddit.com/r/FastLED/comments/1gkcb6m/fastled_FASTLED_OVERCLOCK_17/

### New in 3.7.7 - RGBW LED Strip Support

![image (1)](https://github.com/user-attachments/assets/d4892626-3dc6-4d6d-a740-49ddad495fa5)

</details>

## 🌍 Platform Support

### Platform Categories
| **Arduino Family** | **ESP32 Series** | **Teensy** | **ARM** | **Specialty** |
|--------------------|------------------|------------|---------|---------------|
| Uno, Nano, Mega<br/>Due, Giga R1, R4 | ESP32, S2, S3, C3<br/>C6, H2, P4 | 3.0, 3.1, 4.0, 4.1<br/>LC + OctoWS2811 | STM32, NRF52<br/>Apollo3, Silicon Labs | Raspberry Pi<br/>WASM, x86 |

**FastLED supports 50+ platforms!** From sub-$1 ATtiny chips to high-end Teensy 4.1 with 50 parallel outputs.

## 📦 Installation

### Quick Install Options

| **Arduino IDE** | **PlatformIO** | **Package Managers** |
|-----------------|----------------|----------------------|
| Library Manager → Search "FastLED" → Install | `lib_deps = fastled/FastLED` | `pio pkg install --library "fastled/FastLED"` |
| Or install [latest release .zip](https://github.com/FastLED/FastLED/releases/latest/) | Add to `platformio.ini` | Command line installation |

### Template Projects

- **🎯 [Arduino + PlatformIO Starter](https://github.com/FastLED/PlatformIO-Starter)** - Best of both worlds, works with both IDEs
- **🚀 [FastLED Examples](examples)** - 100+ ready-to-run demos  
- **🌐 [Web Compiler](https://zackees.github.io/fastled-wasm/)** - Test in your browser

<details>
<summary><strong>Arduino IDE Setup Instructions</strong> (Click to expand)</summary>

After installing the Arduino IDE, add FastLED through the Library Manager:

![Arduino IDE Library Manager](https://github.com/user-attachments/assets/b1c02cf9-aba6-4f80-851e-78df914e2501)

![FastLED Library Search](https://github.com/user-attachments/assets/508eb700-7dd4-4901-a901-68c56cc4d0e1)

</details>


## 📚 Documentation & Support

| **📖 Documentation** | **💬 Community** | **🐛 Issues** | **📺 Examples** |
|---------------------|------------------|---------------|-----------------|
| [API Reference](http://fastled.io/docs) | [Reddit r/FastLED](https://www.reddit.com/r/FastLED/) | [GitHub Issues](http://fastled.io/issues) | [Live Demos](https://zackees.github.io/fastled-wasm/) |
| [Doxygen Docs](https://fastled.io/docs/files.html) | 1000s of users & solutions | Bug reports & feature requests | [GitHub Examples](examples) |

**Need Help?** Visit [r/FastLED](https://reddit.com/r/FastLED) - thousands of knowledgeable users and extensive solution history!

## 🎮 Advanced Features

### Performance Leaders: Parallel Output Records

| **Platform** | **Max Parallel Outputs** | **Performance Notes** |
|--------------|---------------------------|----------------------|
| **Teensy 4.1** | 50 parallel strips | Current record holder - [Example](examples/SpecialDrivers/Teensy/ObjectFLED/TeensyMassiveParallel/TeensyMassiveParallel.ino) |
| **Teensy 4.0** | 42 parallel strips | High-performance ARM Cortex-M7 |
| **ESP32DEV** | 24 via I2S + 8 via RMT | [I2S Example](examples/SpecialDrivers/ESP/I2S/EspI2SDemo/EspI2SDemo.ino) |
| **ESP32-S3** | 16 via LCD/I2S + 4 via RMT | [LCD/I80 Example](examples/SpecialDrivers/ESP/LCD_I80/LCD_I80.ino) (NEW!), [I2S Example](examples/SpecialDrivers/ESP/I2S/Esp32S3I2SDemo/Esp32S3I2SDemo.ino) |
| **ESP32-P4** | 16 via LCD (RGB or I80) + RMT | [See LCD docs](#-esp32-lcd-driver---new-memory-efficient-parallel-output-using-lcd-peripheral-recommended) - Supports both RGB and I80 modes |

*Note: The new **LCD driver** is recommended over I2S for better Serial debugging support and memory efficiency. ESP32-P4 supports both RGB and I80 LCD modes. Some ESP32 Arduino core versions (3.10+) have compatibility issues with I2S. Older versions work better - see [issue #1903](https://github.com/FastLED/FastLED/issues/1903)*

### Wiring Best Practices for High-Parallel Setups

**Applies to:** Teensy/ESP32/etc. driving WS2812/WS2813/SK6812 (1-wire) and APA102/clocked chips

<details>
<summary><b>📐 Critical Wiring Rules</b> - Eliminate flickering and corruption (Click to expand)</summary>

#### 1. Star/Bus Ground Architecture
- Tie every ground together: controller GND, each strip GND, PSU GNDs, shield/braid
- Use low-impedance connections (soldered/crimped, not tape-only)
- Keep ground paths short
- Verify: <20mV DC drop between controller and strip GNDs under load

**Why:** Data line voltage is measured against controller ground. Weak ground = corrupt bits.

#### 2. Twisted Pair: Data + Ground
- Run each data line twisted with its own ground wire end-to-end
- Use Cat5/6: one pair per output (data on one wire, other wire as return)
- Bond shield to GND at controller side if using shielded cable

**Why:** Minimizes EMI pickup/emissions and preserves signal integrity over distance.

#### 3. Level Shifting to 5V Logic
- 3.3V microcontrollers need level shifting for 5V LED strips
- Use **74HCT245** (8ch), **74HCT541** (8ch), or **74HCT125/126** (4ch) buffers
- Place **33–220Ω series resistors** at buffer output (before twisted pair)
- Keep buffers close to controller

**Why:** Restores full-swing logic and reduces ringing/reflections.

#### Example: 16-Output Teensy Setup
```
Grounds → ground bus → Teensy GND
Level shifters: 2× 74HCT245 (16 channels), powered at 5V
Per channel: Buffer → 33–100Ω → twisted pair → strip DIN/GND
At strip start: 1000µF cap across +5V/GND
Power injection as needed; keep data/power runs separate
```

#### Troubleshooting Tips
- **Signal issues?** Try underclocking: `FASTLED_OVERCLOCK=0.8` in `platformio.ini` build flags
- **Long runs failing?** Insert dummy LED near controller (set to black, offset array index)
- **Intermittent glitches?** Ensure all PSUs share the same AC circuit

#### Quick Do/Don't
- ✅ **Do:** Star/bus ground, twisted pairs, HCT buffers, source resistors, big cap at strip
- ❌ **Don't:** Float grounds, share one ground for many runs, use unbuffered 3.3V for long 5V lines

Following these practices eliminates 90% of "random sparkle/corruption" issues.

</details>

### Exotic Setups (120+ Outputs!)
**Custom shift register boards** can achieve extreme parallel output:
- **ESP32DEV**: [120-output virtual driver](https://github.com/hpwit/I2SClocklessVirtualLedDriver)
- **ESP32-S3**: [120-output S3 driver](https://github.com/hpwit/I2SClockLessLedVirtualDriveresp32s3) 

## 🚀 Platform-Specific Driver Configuration

---

## Teensy Platform

<details>
<summary><b>⚡ ObjectFLED Driver</b> - DEFAULT driver for Teensy 4.x with advanced parallel output</summary>

### Overview
The `ObjectFLED` driver is an advanced parallel output driver specifically optimized for Teensy 4.0 and 4.1 boards when using WS2812 LEDs. It provides **massive parallel output capacity** and is **enabled by default** on Teensy 4.x - no configuration needed!

### Parallel Output Capacity
- **Teensy 4.1**: Up to **50 parallel strips**
- **Teensy 4.0**: Up to **42 parallel strips**
- **Performance**: ~7x more strips than OctoWS2811

### Default Usage
```cpp
#include <FastLED.h>

CRGB leds[NUM_LEDS];
void setup() {
    // ObjectFLED is used automatically on Teensy 4.x
    FastLED.addLeds<WS2812, DATA_PIN, GRB>(leds, NUM_LEDS);
}
```

### Disabling ObjectFLED (Reverting to Legacy Driver)
If you encounter compatibility issues or wish to use the standard clockless driver instead of `ObjectFLED`, you can disable it by defining `FASTLED_NOT_USES_OBJECTFLED` before including the FastLED header:

```cpp
#define FASTLED_NOT_USES_OBJECTFLED
#include <FastLED.h>
```

### Re-enabling ObjectFLED (Explicitly)
While `ObjectFLED` is the default for Teensy 4.0/4.1, you can explicitly enable it (though not strictly necessary) using:

```cpp
#define FASTLED_USES_OBJECTFLED
#include <FastLED.h>
```

</details>

<details>
<summary><b>🎯 WS2812Serial Driver</b> - High-performance serial-based LED control</summary>

### Overview
The `WS2812Serial` driver leverages serial ports for LED data transmission on Teensy boards, providing non-blocking operation and precise timing.

### Configuration
To use this driver, you must define `USE_WS2812SERIAL` before including the FastLED header.

```cpp
#define USE_WS2812SERIAL
#include <FastLED.h>
```

Then, use `WS2812SERIAL` as the chipset type in your `addLeds` call. The data pin is not used for this driver.

```cpp
FastLED.addLeds<WS2812SERIAL, /* DATA_PIN */, GRB>(leds, NUM_LEDS);
```

### Supported Pins & Serial Ports

| Port    | Teensy LC   | Teensy 3.2 | Teensy 3.5 | Teensy 3.6 | Teensy 4.0 | Teensy 4.1 |
| :------ | :---------: | :--------: | :--------: | :--------: | :--------: | :--------: |
| Serial1 | 1, 4, 5, 24 | 1, 5       | 1, 5, 26   | 1, 5, 26   | 1          | 1, 53      |
| Serial2 |             | 10, 31     | 10         | 10         | 8          | 8          |
| Serial3 |             | 8          | 8          | 8          | 14         | 14         |
| Serial4 |             |            | 32         | 32         | 17         | 17         |
| Serial5 |             |            | 33         | 33         | 20, 39     | 20, 47     |
| Serial6 |             |            | 48         |            | 24         | 24         |
| Serial7 |             |            |            |            | 29         | 29         |
| Serial8 |             |            |            |            |            | 35         |

</details>

---

## ESP32 Platform

<details>
<summary><b>🚀 ESP32 LCD Driver</b> - NEW! Memory-efficient parallel output using LCD peripheral (RECOMMENDED)</summary>

### Overview

The **LCD driver** is a high-performance parallel output driver for ESP32 boards with LCD peripheral support. It uses hardware LCD peripherals to drive up to **16 parallel WS28xx LED strips** with automatic chipset timing optimization.

**Two Driver Variants:**
- **I80 Mode**: Uses LCD_CAM/I80 peripheral with 3-word-per-bit encoding (6 bytes per bit)
- **RGB Mode**: Uses RGB LCD controller with 4-pixel-per-bit encoding (8 bytes per bit)

**Key Advantages over I2S:**
- ✅ **More WS282x Chipsets**: Better support for WS2811, WS2816, WS2813, etc.
- ✅ **Better timing accuracy**: Automatic PCLK optimization per chipset
- ✅ **Serial output works**: Unlike I2S, you can use Serial.print() for debugging
- ✅ **Platform-agnostic**: Automatically detects and uses available LCD peripheral

### Supported Platforms
- **ESP32-S3**: LCD_CAM peripheral with I80 interface (3-word encoding)
- **ESP32-P4**: Both RGB LCD controller (4-pixel encoding) AND I80 interface (3-word encoding)
- **Future ESP32 variants**: Any chip with RGB or I80 LCD peripheral support

### Configuration
To use the LCD driver instead of I2S, define `FASTLED_ESP32_LCD_DRIVER` before including FastLED:

```cpp
#define FASTLED_ESP32_LCD_DRIVER
#include <FastLED.h>

CRGB leds1[NUM_LEDS];
CRGB leds2[NUM_LEDS];

void setup() {
    // Standard FastLED API - LCD driver auto-selected
    FastLED.addLeds<WS2812, 2>(leds1, NUM_LEDS);  // WS2812 on pin 2
    FastLED.addLeds<WS2811, 4>(leds2, NUM_LEDS);  // WS2811 on pin 4 (slower timing)
}

void loop() {
    // Your LED code here
    FastLED.show();
}
```

**Without the define**, FastLED defaults to the I2S driver (legacy behavior).

### Performance
- **Up to 16 parallel strips** (all must use the same chipset)
- **Memory usage**:
  - I80 (S3): 144 KB per 1000 LEDs (6 bytes per bit)
  - RGB (P4): 192 KB per 1000 LEDs (8 bytes per bit)
- **Recommended for new projects** on ESP32-S3/P4

### Examples
- [Esp32S3I80 Example](examples/Esp32S3I80/) - Demonstrates LCD/I80 driver usage

**Note:** All parallel strips must use the same chipset timing. For per-strip timing control, use the RMT driver.

</details>

<details>
<summary><b>🔧 ESP32-S3 I2S Driver</b> - Legacy parallel output using I2S peripheral</summary>

### Overview

The ESP32-S3 I2S driver leverages the I2S peripheral for high-performance parallel WS2812 output on ESP32-S3 boards. This driver is a dedicated clockless implementation.

**Note:** The **LCD driver** (above) is now recommended for new projects on ESP32-S3 due to better memory efficiency and Serial debugging support.

### Configuration
To use this driver, you must define `FASTLED_USES_ESP32S3_I2S` before including the FastLED header.

```cpp
#define FASTLED_USES_ESP32S3_I2S
#include <FastLED.h>
```

Then, use `WS2812` as the chipset type in your `addLeds` call. The data pin will be configured by the I2S driver.

```cpp
FastLED.addLeds<WS2812, DATA_PIN, GRB>(leds, NUM_LEDS);
```

**Note:** This driver requires a compatible Arduino-ESP32 core/IDF.

</details>

<details>
<summary><b>📡 Generic ESP32 I2S Driver</b> - Universal I2S support for ESP32</summary>

### Overview
The generic ESP32 I2S driver provides parallel WS2812 output for various ESP32 boards (e.g., ESP32-DevKitC). It uses the I2S peripheral for efficient data transmission.

### Configuration
To use this driver, you must define `FASTLED_ESP32_I2S` before including the FastLED header.

```cpp
#define FASTLED_ESP32_I2S
#include <FastLED.h>
```

Then, use `WS2812` as the chipset type in your `addLeds` call:

```cpp
FastLED.addLeds<WS2812, DATA_PIN, GRB>(leds, NUM_LEDS);
```

### DMA Buffer Configuration
For improved resilience under interrupt load (e.g., Wi-Fi activity), you can increase the number of I2S DMA buffers:

```cpp
#define FASTLED_ESP32_I2S_NUM_DMA_BUFFERS 4
```

</details>

<details>
<summary><b>⚠️ ESP32 RMT Driver Configuration</b> - Critical performance and compatibility information</summary>

### ⚡ Performance Alert

**Important:** The RMT4 and RMT5 drivers use completely different APIs. FastLED's custom RMT4 driver is specifically optimized for LED control and significantly outperforms Espressif's generic RMT5 wrapper in terms of:
- **Performance**: Lower interrupt overhead and better timing precision
- **Wi-Fi resistance**: Dual-buffer design prevents flickering under Wi-Fi load
- **Efficiency**: Direct hardware control vs. abstraction layer overhead
- **Strip capacity**: RMT4 uses worker pools to support more LED strips; RMT5 lacks worker pools and is limited by hardware channels (particularly restrictive on ESP32-C3 with only 2 TX channels)

FastLED includes two RMT driver implementations:
- **RMT4 driver**: FastLED's custom, highly-optimized driver with direct RMT peripheral control
- **RMT5 driver**: Wrapper around Espressif's `led_strip` component (ESP-IDF 5.0+)

By default, FastLED automatically selects:
- **ESP-IDF 5.0+**: Uses RMT5 driver (for compatibility)
- **ESP-IDF 4.x and earlier**: Uses RMT4 driver

### Forcing RMT4 Driver (Recommended for Performance)

To force the use of FastLED's optimized RMT4 driver, you have two options:

**Option 1: Arduino IDE - Downgrade ESP32 Core**
- Use any ESP32 Arduino Core version below 3.0.
- These versions use ESP-IDF 4.x and below which defaults to RMT4

**Option 2: PlatformIO - Set Build Flag**

For ESP-IDF 5.0+ environments, set `FASTLED_RMT5=0` as a build flag:

```ini
[env:esp32]
platform = espressif32
board = esp32dev
framework = arduino
lib_deps = fastled/FastLED
build_flags =
    -D FASTLED_RMT5=0
```

**📚 PlatformIO Users:** For a complete quick start guide with FastLED and PlatformIO, check out our official starter template: [github.com/fastled/platformio-starter](https://github.com/fastled/platformio-starter)

**⚠️ Critical:** RMT4 and RMT5 drivers cannot coexist in the same sketch. Espressif's ESP-IDF will cause a boot crash if it detects both drivers attempting to initialize. You must choose one or the other at compile time.

**Note:** Using RMT4 is strongly recommended if you experience flickering with Wi-Fi enabled or need maximum LED performance. RMT4 uses ESP-IDF's legacy RMT APIs which are still maintained for backward compatibility in ESP-IDF 5.x, though marked as deprecated.

</details>

---

## Silicon Labs (MGM240/EFR32MG24) Platform

<details>
<summary><b>🔧 ezWS2812 GPIO Driver</b> - Optimized bit-banging with cycle-accurate timing</summary>

### Overview
The `EZWS2812_GPIO` driver uses optimized GPIO bit-banging with cycle-accurate timing calibrated for 39MHz and 78MHz CPU frequencies. This driver is **always available** and requires no hardware peripherals.

### Performance
- ~400Hz refresh rate
- No hardware peripheral consumption
- Optimized assembly-level timing

### Usage
```cpp
#include <FastLED.h>

CRGB leds[NUM_LEDS];
void setup() {
    // GPIO-based controller on pin 7
    FastLED.addLeds<EZWS2812_GPIO, 7, GRB>(leds, NUM_LEDS);
}
```

</details>

<details>
<summary><b>⚡ ezWS2812 SPI Driver</b> - Hardware-accelerated maximum performance</summary>

### Overview
The `EZWS2812_SPI` driver provides hardware SPI acceleration for maximum performance (~1000Hz refresh rate). This driver **consumes a hardware SPI peripheral** and must be explicitly enabled.

### Why Opt-In Required
Following the same pattern as ObjectFLED for Teensy, the SPI controller must be explicitly enabled to prevent accidentally consuming the SPI peripheral that may be needed for other components (SD cards, displays, sensors).

### Configuration
**Required define before including FastLED:**
```cpp
#define FASTLED_USES_EZWS2812_SPI  // MUST be before #include
#include <FastLED.h>

CRGB leds[NUM_LEDS];
void setup() {
    // SPI-based controller (consumes hardware SPI)
    FastLED.addLeds<EZWS2812_SPI, GRB>(leds, NUM_LEDS);
}
```

### Performance
- ~1000Hz refresh rate (2.5x faster than GPIO)
- Hardware-accelerated
- Consumes one SPI peripheral

</details>

<details>
<summary><b>ℹ️ Important Notes</b> - Compatibility and limitations</summary>

### Supported Boards
- Arduino Nano Matter
- SparkFun Thing Plus Matter
- Seeed Xiao MG24 Sense

### LED Compatibility
The `EZWS2812_GPIO` and `EZWS2812_SPI` controllers are optimized specifically for **WS2812 LEDs only**. For other LED chipsets (SK6812, TM1809, UCS1903, etc.), FastLED automatically uses the generic clockless driver which supports all timing variants but with standard performance.

### Driver Source
These drivers are based on the Silicon Labs ezWS2812 library and provide seamless FastLED API compatibility with enhanced performance for MG24-based boards.

</details>

**Note:** All I2S lanes must share the same chipset/timings. If per-lane timing differs, consider using the RMT driver instead. 

### Supported LED Chipsets

FastLED supports virtually every LED chipset available:

| **Clockless (3-wire)** | **SPI-based (4-wire)** | **Specialty** |
|------------------------|------------------------|---------------|
| **WS281x Family**: WS2811, WS2812 (NeoPixel), WS2812-V5B, WS2815 | **APA102 / DotStars**: Including HD107s (40MHz turbo) | **SmartMatrix Panels** |
| **TM180x Series**: TM1809/4, TM1803 | **High-Speed SPI**: LPD8806, WS2801, SM16716 | **DMX Output** |
| **Other 3-wire**: UCS1903, GW6205, SM16824E | **APA102HD**: Driver-level gamma correction | **P9813 Total Control** |

**RGBW Support**: WS2816 and other white-channel LED strips • **Overclocking**: WS2812 up to 70% speed boost

More details: [Chipset Reference Wiki](https://github.com/FastLED/FastLED/wiki/Chipset-reference)

### APA102 High Definition Mode
```cpp
#define LED_TYPE APA102HD  // Enables hardware gamma correction
void setup() {
  FastLED.addLeds<LED_TYPE, DATA_PIN, CLOCK_PIN, RGB>(leds, NUM_LEDS);
}
```

![APA102HD Comparison](https://github.com/user-attachments/assets/999e68ce-454f-4f15-9590-a8d2e8d47a22)

Read more: [APA102 HD Documentation](APA102.md) • [Rust Implementation](https://docs.rs/apa102-spi/latest/apa102_spi/)

## 🛠️ Development & Contributing

[![clone and compile](https://github.com/FastLED/FastLED/actions/workflows/build_clone_and_compile.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_clone_and_compile.yml)

**Ready to contribute?** FastLED welcomes code contributions, platform testing, documentation improvements, and community support.

<details>
<summary>🔧 <strong>Development Setup & Contributing Guide</strong> (Click to expand)</summary>

### Quick Development Setup
Zero pain setup - can be done from command line in seconds with `uv` or `python`:

![FastLED Development Setup](https://github.com/user-attachments/assets/f409120e-be6f-4158-81b5-d4bf935833b2)

**Steps:**
1. Fork the repository on GitHub
2. Clone your fork locally: `git clone https://github.com/yourusername/FastLED.git`  
3. Test compilation: `bash compile` (select your target platform)
4. Make your changes and test
5. Push to your fork and create a pull request

**See our detailed [Contributing Guide](CONTRIBUTING.md) for more information.**

### Testing Other Devices
Use the `compile` script to test on 20+ platforms:
- Teensy series, ESP32 variants, Arduino family, ARM boards, and more
- Automated testing ensures compatibility across all supported platforms

### How to Help

| **📋 How to Help** | **🔗 Resources** |
|-------------------|------------------|
| **Code contributions**: Bug fixes, new features, optimizations | [Contributing Guide](CONTRIBUTING.md) |  
| **Platform support**: Help test on new/existing platforms | [Platform Testing](compile) |
| **Documentation**: Improve examples, fix typos, add tutorials | [Documentation](http://fastled.io/docs) |
| **Community**: Answer questions on Reddit, report issues | [r/FastLED](https://reddit.com/r/FastLED) |

**Platform Porting**: Information on porting FastLED to new platforms: [PORTING.md](PORTING.md)

### About FastLED

**What's in the name?** Originally "FastSPI_LED" focused on high-speed SPI, but evolved to support all LED types and became "FastLED" - everything fast, for LEDs.

**Official Site**: [fastled.io](http://fastled.io) - documentation, issues, and news

</details>

---

## Daniel Garcia, Founder of FastLED

In Memory of Daniel Garcia
Daniel Garcia, the brilliant founder of FastLED, tragically passed away in September 2019 in the Conception dive boat fire alongside his partner, Yulia. This heartbreaking loss was felt deeply by the maker and developer community, where Daniel's contributions had left an indelible mark.

Daniel was more than just a talented programmer; he was a passionate innovator who transformed the way creators interacted with LED technology. His work on FastLED brought high-performance LED control to countless projects, empowering developers to craft breathtaking installations.

In his personal life, Daniel was known for his kindness and creativity. His pride in FastLED and the vibrant community it fostered was a testament to his dedication to open-source development and his commitment to helping others bring light into the world.

While Daniel is no longer with us, his legacy continues through the FastLED library and the countless makers who use it. The community he built serves as a living tribute to his ingenuity, generosity, and the joy he found in sharing his work with the world.

## About the Current Contributor

Zach Vorhies, the current main contributor to FastLED, briefly worked with Dan in 2014 in San Francisco and was an avid user of the FastLED library for over 13 years. After Daniel Garcia’s untimely passing, Zach stepped up to ensure FastLED’s continued growth and development.

Zach has this to say about FastLED:

*"The true power of FastLED lies in its ability to transform programmers into LED artists. Free space becomes their canvas; bending light is their medium. FastLED is a collective effort by programmers who want to manifest the world that science fiction writers promised us. -- To contribute code to FastLED is to leave behind a piece of something immortal."*


---

**💫 In memory of Daniel Garcia and the vision that transforms programmers into LED artists**

*To stay updated on the latest feature releases, please click the `Watch` button in the upper right*
"" 
