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

*MGM240 (EFR32MG24) support for Arduino Nano Matter and SparkFun Thing Plus Matter boards

### Raspberry Pi Pico
[![rp2040](https://github.com/FastLED/FastLED/actions/workflows/build_rp2040.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_rp2040.yml) [![rp2350](https://github.com/FastLED/FastLED/actions/workflows/build_rp2350.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_rp2350.yml) [![rp2350B SparkfunXRP](https://github.com/FastLED/FastLED/actions/workflows/build_rp2350B.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_rp2350B.yml)

### ESP32 (Espressif)
**ESP8266:** [![esp32-8266](https://github.com/FastLED/FastLED/actions/workflows/build_esp8622.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_esp8622.yml)

**ESP32 Classic:** [![esp32dev](https://github.com/FastLED/FastLED/actions/workflows/build_esp32dev.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_esp32dev.yml) [![esp32wroom](https://github.com/FastLED/FastLED/actions/workflows/build_esp32wroom.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_esp32wroom.yml)

**ESP32 S-Series:** [![esp32s2](https://github.com/FastLED/FastLED/actions/workflows/build_esp32s2.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_esp32s2.yml) [![esp32s3](https://github.com/FastLED/FastLED/actions/workflows/build_esp32s3.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_esp32s3.yml)

**ESP32 C-Series:** [![esp32c2](https://github.com/FastLED/FastLED/actions/workflows/build_esp32c2.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_esp32c2.yml)* [![esp32c3](https://github.com/FastLED/FastLED/actions/workflows/build_esp32c3.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_esp32c3.yml) [![esp32c5](https://github.com/FastLED/FastLED/actions/workflows/build_esp32c5.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_esp32c5.yml)[![esp32c6](https://github.com/FastLED/FastLED/actions/workflows/build_esp32c6.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_esp32c6.yml)

**ESP32 Advanced:** [![esp32h2](https://github.com/FastLED/FastLED/actions/workflows/build_esp32h2.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_esp32h2.yml) [![esp32p4](https://github.com/FastLED/FastLED/actions/workflows/build_esp32p4.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_esp32p4.yml)

**Special Features:** [![esp32_i2s_ws2812](https://github.com/FastLED/FastLED/actions/workflows/build_esp32_i2s_ws2812.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_esp32_i2s_ws2812.yml) [![esp32 extra libs](https://github.com/FastLED/FastLED/actions/workflows/build_esp_extra_libs.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_esp_extra_libs.yml) [![esp32dev_namespace](https://github.com/FastLED/FastLED/actions/workflows/build_esp32dev_namespace.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_esp32dev_namespace.yml)

**QEMU Tests:** [![ESP32-DEV QEMU Test](https://github.com/FastLED/FastLED/actions/workflows/qemu_esp32dev_test.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/qemu_esp32dev_test.yml) [![ESP32-C3 QEMU Test](https://github.com/FastLED/FastLED/actions/workflows/qemu_esp32c3_test.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/qemu_esp32c3_test.yml) [![ESP32-S3 QEMU Test](https://github.com/FastLED/FastLED/actions/workflows/qemu_esp32s3_test.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/qemu_esp32s3_test.yml)

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
| **Teensy 4.1** | 50 parallel strips | Current record holder - [Example](examples/TeensyMassiveParallel/TeensyMassiveParallel.ino) |
| **Teensy 4.0** | 42 parallel strips | High-performance ARM Cortex-M7 |
| **ESP32DEV** | 24 via I2S + 8 via RMT | [I2S Example](examples/EspI2SDemo/EspI2SDemo.ino) |
| **ESP32-S3** | 16 via I2S + 4 via RMT | [S3 Example](examples/Esp32S3I2SDemo/Esp32S3I2SDemo.ino) |

*Note: Some ESP32 Arduino core versions (3.10+) have compatibility issues. Older versions work better - see [issue #1903](https://github.com/FastLED/FastLED/issues/1903)*

### Exotic Setups (120+ Outputs!)
**Custom shift register boards** can achieve extreme parallel output:
- **ESP32DEV**: [120-output virtual driver](https://github.com/hpwit/I2SClocklessVirtualLedDriver)
- **ESP32-S3**: [120-output S3 driver](https://github.com/hpwit/I2SClockLessLedVirtualDriveresp32s3) 

### Parallel WS2812 Drivers

FastLED supports several drivers for parallel WS2812 output.

#### Teensy

The following drivers are available for Teensy boards:

##### WS2812Serial driver

The `WS2812Serial` driver leverages serial ports for LED data transmission on Teensy boards.

###### Usage
To use this driver, you must define `USE_WS2812SERIAL` before including the FastLED header.

```cpp
#define USE_WS2812SERIAL
#include <FastLED.h>
```

Then, use `WS2812SERIAL` as the chipset type in your `addLeds` call. The data pin is not used for this driver.

```cpp
FastLED.addLeds<WS2812SERIAL, /* DATA_PIN */, GRB>(leds, NUM_LEDS);
```

###### Supported Pins & Serial Ports

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

##### ObjectFLED Driver

The `ObjectFLED` driver is an advanced parallel output driver specifically optimized for Teensy 4.0 and 4.1 boards when using WS2812 LEDs. It is designed to provide high-performance, multi-pin output.

By default, FastLED automatically uses the `ObjectFLED` driver for WS2812 LEDs on Teensy 4.0/4.1 boards.

###### Disabling ObjectFLED (Reverting to Legacy Driver)
If you encounter compatibility issues or wish to use the standard clockless driver instead of `ObjectFLED`, you can disable it by defining `FASTLED_NOT_USES_OBJECTFLED` before including the FastLED header:

```cpp
#define FASTLED_NOT_USES_OBJECTFLED
#include <FastLED.h>
```

###### Re-enabling ObjectFLED (Explicitly)
While `ObjectFLED` is the default for Teensy 4.0/4.1, you can explicitly enable it (though not strictly necessary) using:

```cpp
#define FASTLED_USES_OBJECTFLED
#include <FastLED.h>
```

#### ESP32

The following drivers are available for ESP32 boards:

##### ESP32-S3 I2S Driver

The `ESP32-S3 I2S` driver leverages the I2S peripheral for high-performance parallel WS2812 output on ESP32-S3 boards. This driver is a dedicated clockless implementation.

###### Usage
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

##### Generic ESP32 I2S Driver

The generic `ESP32 I2S` driver provides parallel WS2812 output for various ESP32 boards (e.g., ESP32-DevKitC). It uses the I2S peripheral for efficient data transmission.

###### Usage
To use this driver, you must define `FASTLED_ESP32_I2S` before including the FastLED header.

```cpp
#define FASTLED_ESP32_I2S
#include <FastLED.h>
```

Then, use `WS2812` as the chipset type in your `addLeds` call. The data pin will be configured by the I2S driver.

```cpp
FastLED.addLeds<WS2812, DATA_PIN, GRB>(leds, NUM_LEDS);
```

###### DMA Buffer Configuration
For improved resilience under interrupt load (e.g., Wi-Fi activity), you can increase the number of I2S DMA buffers by defining `FASTLED_ESP32_I2S_NUM_DMA_BUFFERS`. A value of `4` is often recommended.

```cpp
#define FASTLED_ESP32_I2S_NUM_DMA_BUFFERS 4
```

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
