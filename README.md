FastLED Library
===========



[![arduino-library-badge](https://www.ardu-badge.com/badge/FastLED.svg)](https://www.ardu-badge.com/FastLED)
[![build status](https://github.com/FastLED/FastLED/workflows/build/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build.yml)
[![unit tests](https://github.com/FastLED/FastLED/actions/workflows/build_unit_test.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_unit_test.yml)
[![Arduino Library Lint](https://github.com/FastLED/FastLED/actions/workflows/arduino_library_lint.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/arduino_library_lint.yml)
[![Documentation](https://img.shields.io/badge/Docs-Doxygen-blue.svg)](http://fastled.io/docs)
[![Reddit](https://img.shields.io/badge/reddit-/r/FastLED-orange.svg?logo=reddit)](https://www.reddit.com/r/FastLED/)

Want to control a strip of leds? Or control 10's of thousands? FastLED has your back.

FastLED is a robust and massively parallel-led driver for Arduino, Esp32, RaspberryPi,  Atmega, Teensy, Uno, Apollo3 Arm and more. Also runs on dirt cheap sub $1 devices, due to it's incredibly small compile size. High end devices can drive upto ~30k LEDS (Teensy) and ~20k on ESP32. Supports nearly every single LED chipset in existence. Background rendering (ESP32/Teensy/RaspberriPi) means you can respond to user input while the leds render. FastLED is the fourth [most popular library on Arduino](https://docs.arduino.cc/libraries/).


## Star History

<a href="https://star-history.com/#fastled/fastled&Date">
 <picture>
   <source media="(prefers-color-scheme: dark)" srcset="https://api.star-history.com/svg?repos=fastled/fastled&type=Date&theme=dark" />
   <source media="(prefers-color-scheme: light)" srcset="https://api.star-history.com/svg?repos=fastled/fastled&type=Date" />
   <img alt="Star History Chart" src="https://api.star-history.com/svg?repos=fastled/fastled&type=Date" />
 </picture>
</a>


## About

This is a driver library for easily & efficiently controlling a wide variety of LED chipsets, like the ones
sold by Adafruit (NeoPixel, DotStar, LPD8806), Sparkfun (WS2801), and AliExpress.

The 3.9.x series introduced:
  * Massive parallel rendering to drive thousands of LEDs.
  * Background rendering of LEDs so that your program/sketch can prepare the next frame and respond to user input without affect frame rate.

In addition to writing to the LEDs, this library also includes a number of functions for high-performing 8-bit math for manipulating
your RGB values, as well as low level classes for abstracting out access to pins and SPI hardware, while
still keeping things as fast as possible.

We have multiple goals with this library:

* Quick start for new developers - hook up your LEDs and go, no need to think about specifics of the LED chipsets being used
* Zero pain switching LED chipsets - you get some new LEDs that the library supports, just change the definition of LEDs you're using, et. voila!  Your code is running with the new LEDs.
* High performance - with features like zero cost global brightness scaling, high performance 8-bit math for RGB manipulation, and some of the fastest bit-bang'd SPI support around, FastLED wants to keep as many CPU cycles available for your LED patterns as possible

## Example

*This is an Arduino Sketch that will run on Arduino Uno/Esp32/Raspberri Pi*
```C++
// New feature! Overclocking WS2812
// #define FASTLED_OVERCLOCK 1.2 // 20% overclock ~ 960 khz.
#include <FastLED.h>
#define NUM_LEDS 60
#define DATA_PIN 6
CRGB leds[NUM_LEDS];
void setup() { FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS); }
void loop() {
	leds[0] = CRGB::White; FastLED.show(); delay(30);
	leds[0] = CRGB::Black; FastLED.show(); delay(30);
}
```

For more examples see this [link](examples). Web compiled [examples](https://zackees.github.io/fastled-wasm/).


# New Feature Announcements

## New in 3.9.13: HD107 "Turbo" 40Mhz LED Support

![image](https://github.com/user-attachments/assets/9684ab7d-2eaa-40df-a00d-0dff18098917)

## New in 3.9.12: WS2816 "HD" LED support

![image](https://github.com/user-attachments/assets/258ec44c-af82-44b7-ad7b-fac08daa9bcb)

## New in 3.9.10: Super Stable WS2812 SPI driver for ESP32

![image (2)](https://github.com/user-attachments/assets/b3c5801c-66df-40af-a6b8-bbd1520fbb36)

## New in 3.9.9: 16-way Yves I2S parallel driver for the ESP32-S3

![perpetualmaniac_an_led_display_in_a_room_lots_of_refaction_of_t_eb7c170a-7b2c-404a-b114-d33794b4954b](https://github.com/user-attachments/assets/982571fc-9b8d-4e58-93be-5bed76a0c53d)


## New in 3.9.8 - Massive Teensy 4.1 & 4.0 LED output
![New Project](https://github.com/user-attachments/assets/79dc2801-5161-4d5a-90a2-0126403e215f)


## New in 3.9.2 - Overclocking of WS2812
![image](https://github.com/user-attachments/assets/be98fbe6-0ec7-492d-8ed1-b7eb6c627e86)
Update: max overclock has been reported at +70%: https://www.reddit.com/r/FastLED/comments/1gkcb6m/fastled_FASTLED_OVERCLOCK_17/

## Star History

<a href="https://star-history.com/#fastled/fastled&Date">
 <picture>
   <source media="(prefers-color-scheme: dark)" srcset="https://api.star-history.com/svg?repos=fastled/fastled&type=Date&theme=dark" />
   <source media="(prefers-color-scheme: light)" srcset="https://api.star-history.com/svg?repos=fastled/fastled&type=Date" />
   <img alt="Star History Chart" src="https://api.star-history.com/svg?repos=fastled/fastled&type=Date" />
 </picture>
</a>


## Supported Platforms
### Arduino

[![uno](https://github.com/FastLED/FastLED/actions/workflows/build_uno.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_uno.yml)

[![attiny85](https://github.com/FastLED/FastLED/actions/workflows/build_attiny85.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_attiny85.yml)

[![attiny88](https://github.com/FastLED/FastLED/actions/workflows/build_attiny88.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_attiny88.yml)

[![attiny1604](https://github.com/FastLED/FastLED/actions/workflows/build_attiny1604.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_attiny1604.yml)


[![attiny1616](https://github.com/FastLED/FastLED/actions/workflows/build_attiny1616.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_attiny1616.yml)


[![attiny4313](https://github.com/FastLED/FastLED/actions/workflows/build_attiny4313.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_attiny4313.yml)
*Now compiles in the upcoming FastLED 3.9.14! - Very memory limited, so only tested against examples WS2812 Blink and APA102*


[![yun](https://github.com/FastLED/FastLED/actions/workflows/build_yun.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_yun.yml)


[![digix](https://github.com/FastLED/FastLED/actions/workflows/build_digix.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_digix.yml)


[![uno_r4_wifi](https://github.com/FastLED/FastLED/actions/workflows/build_uno_r4_wifif.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_uno_r4_wifif.yml)


[![nano_every](https://github.com/FastLED/FastLED/actions/workflows/build_nano_every.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_nano_every.yml)

[![Arduino Giga-R1](https://github.com/FastLED/FastLED/actions/workflows/build_giga_r1.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_giga_r1.yml)
*Multiple issues with this board. Pins aren't defined, F_CPU is not defined nor is it constant. And lots of other issues. Help wanted!*


### Teensy
[![teensy30](https://github.com/FastLED/FastLED/actions/workflows/build_teensy30.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_teensy30.yml)


[![teensy31](https://github.com/FastLED/FastLED/actions/workflows/build_teensy31.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_teensy31.yml)


[![teensyLC](https://github.com/FastLED/FastLED/actions/workflows/build_teensyLC.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_teensyLC.yml)


[![teensy40](https://github.com/FastLED/FastLED/actions/workflows/build_teensy40.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_teensy40.yml)


[![teensy41](https://github.com/FastLED/FastLED/actions/workflows/build_teensy41.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_teensy41.yml)

*Specific Features*

[![teensy_octoWS2811](https://github.com/FastLED/FastLED/actions/workflows/build_teensy_octo.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_teensy_octo.yml)

[![teensy41 ObjectFLED](https://github.com/FastLED/FastLED/actions/workflows/build_teensy41_ofled.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_teensy41_ofled.yml)

### NRF

[![nrf52840_sense](https://github.com/FastLED/FastLED/actions/workflows/build_adafruit_feather_nrf52840_sense.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_adafruit_feather_nrf52840_sense.yml)

[![nordicnrf52_dk](https://github.com/FastLED/FastLED/actions/workflows/build_nrf52840_dk.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_nrf52840_dk.yml)

[![adafruit_xiaoblesense](https://github.com/FastLED/FastLED/actions/workflows/build_adafruit_xiaoblesense.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_adafruit_xiaoblesense.yml)

[![nrf52_xiaoblesense](https://github.com/FastLED/FastLED/actions/workflows/build_nrf52_xiaoblesense.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_nrf52_xiaoblesense.yml)
(This board has mbed engine but doesn't compile against Arduino.h right now for some unknown reason.)

### Apollo3

[![apollo3_red](https://github.com/FastLED/FastLED/actions/workflows/build_apollo3_red.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_apollo3_red.yml) *Board needs pin definitions.*

[![apollo3_thing_explorable](https://github.com/FastLED/FastLED/actions/workflows/build_apollo3_thing_explorable.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_apollo3_thing_explorable.yml) *Now Supported in the upcoming FastLED 3.9.12!*

### STM

[![bluepill](https://github.com/FastLED/FastLED/actions/workflows/build_bluepill.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_bluepill.yml)

[![maple_mini](https://github.com/FastLED/FastLED/actions/workflows/build_maple_map.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_maple_map.yml)

[![stm103tb](https://github.com/FastLED/FastLED/actions/workflows/build_stm103tb.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_stm103tb.yml)
(PlatformIO doesn't support this board yet and we don't know what the build info is to support this is yet)

### Raspberry Pi

[![rp2040](https://github.com/FastLED/FastLED/actions/workflows/build_rp2040.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_rp2040.yml)


[![rp2350](https://github.com/FastLED/FastLED/actions/workflows/build_rp2350.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_rp2350.yml)


### Esp

[![esp32-8266](https://github.com/FastLED/FastLED/actions/workflows/build_esp8622.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_esp8622.yml)


[![esp32dev](https://github.com/FastLED/FastLED/actions/workflows/build_esp32dev.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_esp32dev.yml)


[![esp32wroom](https://github.com/FastLED/FastLED/actions/workflows/build_esp32wroom.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_esp32wroom.yml)


[![esp32c2](https://github.com/FastLED/FastLED/actions/workflows/build_esp32c2.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_esp32c2.yml)
*Now supported as of FastLED 3.9.10!*


[![esp32c3](https://github.com/FastLED/FastLED/actions/workflows/build_esp32c3.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_esp32c3.yml)

[![esp32s2](https://github.com/FastLED/FastLED/actions/workflows/build_esp32s2.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_esp32s2.yml)

[![esp32s3](https://github.com/FastLED/FastLED/actions/workflows/build_esp32s3.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_esp32s3.yml)


[![esp32c6](https://github.com/FastLED/FastLED/actions/workflows/build_esp32c6.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_esp32c6.yml)


[![esp32h2](https://github.com/FastLED/FastLED/actions/workflows/build_esp32h2.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_esp32h2.yml)

*Specific features*

[![esp32_i2s_ws2812](https://github.com/FastLED/FastLED/actions/workflows/build_esp32_i2s_ws2812.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_esp32_i2s_ws2812.yml)

[![esp32 extra libs](https://github.com/FastLED/FastLED/actions/workflows/build_esp_extra_libs.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_esp_extra_libs.yml)

*Legacy Toolchains*

[![esp32dev-idf3.3-lts](https://github.com/FastLED/FastLED/actions/workflows/build_esp32dev_idf3.3.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_esp32dev_idf3.3.yml)

Espressif's current evaluation of FastLED's compatibility with their product sheet can be found [here](https://github.com/espressif/arduino-esp32/blob/gh-pages/LIBRARIES_TEST.md)


### x86

[![linux_native](https://github.com/FastLED/FastLED/actions/workflows/build_linux.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_linux.yml)

### Wasm

[![wasm](https://github.com/FastLED/FastLED/actions/workflows/build_wasm.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_wasm.yml)

[![wasm_compile_test](https://github.com/FastLED/FastLED/actions/workflows/build_wasm_compilers.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_wasm_compilers.yml)

## Compiled Library Size Check

[![attiny85_binary_size](https://github.com/FastLED/FastLED/actions/workflows/check_attiny85.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/check_attiny85.yml)

[![uno_binary_size](https://github.com/FastLED/FastLED/actions/workflows/check_uno_size.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/check_uno_size.yml)


[![esp32dev_binary_size](https://github.com/FastLED/FastLED/actions/workflows/check_esp32_size.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/check_esp32_size.yml)


[![teensy41_binary_size](https://github.com/FastLED/FastLED/actions/workflows/check_teensy41_size.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/check_teensy41_size.yml)

# Install

## Arduino IDE

After the ArduinoIDE is installed then add the library to your IDE

![image](https://github.com/user-attachments/assets/b1c02cf9-aba6-4f80-851e-78df914e2501)

![image](https://github.com/user-attachments/assets/508eb700-7dd4-4901-a901-68c56cc4d0e1)

## PlatformIO

PlatformIO offers an incredible IDE experience. Setup is easier than you think. Follow our guide here. Our template will allow your project to be compiled by both PlatformIO and ArduinoIDE

https://github.com/FastLED/PlatformIO-Starter


# How to maximize the number of parallel WS2812 outputs

Some of the new processors can drive many many WS2812 strips in parallel. 

## Stock Setups

### Teensy 4.0/4.1

This chipset holds the current record for parallel output in a stock configuration. The theoretical output is 50 strips at a time with Teensy 4.1 and 42 strips with Teensy 4.2.

See this [example](https://github.com/FastLED/FastLED/blob/master/examples/TeensyMassiveParallel/TeensyMassiveParallel.ino) on how to enable.

### ESP32DEV

Surprisingly it's the good old ESP32Dev and not the ESP32S3, which holds the esp record for the amount of parallel outputs at 24 through I2S, and 8 via RMT.

I2S needs special setup as of 3.9.11 and earlier (current version of this writing is 3.9.11) see the [example](https://github.com/FastLED/FastLED/blob/master/examples/EspI2SDemo/EspI2SDemo.ino) here.

### ESP32-S3

The S3 is a CPU beast, but has half the RMT tx channels (4) and 2/3rds the I2S channels (16) of ESPDev. The S3 requires a special driver for I2S which you can find in this [example](https://github.com/FastLED/FastLED/blob/master/examples/Esp32S3I2SDemo/Esp32S3I2SDemo.ino)

### RaspberriPi

I (Zach Vorhies) don't use this platform. Help wanted on what the limits of this chip is.


## Exotic Setups

If you are willing to make a custom board with shift registers, then the ESp32S3 and ESP32Dev have special "virtual pin" libraries. These libraries will allow you to drive 120 parallel WS2812 outputs. However these are not included in FastLED but are compatible with it.

  * Esp32DEV: https://github.com/hpwit/I2SClocklessVirtualLedDriver
  * Esp32-S3: https://github.com/hpwit/I2SClockLessLedVirtualDriveresp32s3


## Development

[![clone and compile](https://github.com/FastLED/FastLED/actions/workflows/build_clone_and_compile.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_clone_and_compile.yml)

Zero pain setup and install/test/run. Can be done from the command line in seconds if `uv` or `python` are installed. See our [contributing guide](https://github.com/FastLED/FastLED/blob/master/CONTRIBUTING.md) guide for more information.

![image](https://github.com/user-attachments/assets/f409120e-be6f-4158-81b5-d4bf935833b2)



When changes are made then push to your fork to your repo and git will give you a url to trigger a pull request into the master repo.

### Testing other devices

  * run [compile](compile) and then select your board

```bash
Available boards:
[0]: ATtiny1616
[1]: adafruit_feather_nrf52840_sense
[2]: attiny85
[3]: bluepill
[4]: digix
[5]: esp01
[6]: esp32c2
[7]: esp32c3
[8]: esp32c6
[9]: esp32s3
[10]: esp32dev
[11]: esp32dev_i2s
[12]: esp32dev_idf44
[13]: esp32rmt_51
[14]: nano_every
[15]: rpipico
[16]: rpipico2
[17]: teensy30
[18]: teensy41
[19]: uno
[20]: uno_r4_wifi
[21]: xiaoblesense_adafruit
[22]: yun
[all]: All boards
Enter the number of the board you want to use: 0
```

## Help and Support

If you need help with using the library, please consider visiting the Reddit community at https://reddit.com/r/FastLED. There are thousands of knowledgeable FastLED users in that group and a plethora of solutions in the post history.

If you are looking for documentation on how something in the library works, please see the Doxygen documentation online at http://fastled.io/docs.

If you run into bugs with the library, or if you'd like to request support for a particular platform or LED chipset, please submit an issue at http://fastled.io/issues.

## Supported LED Chipsets

Here's a list of all the LED chipsets are supported.  More details on the LED chipsets are included [on our wiki page](https://github.com/FastLED/FastLED/wiki/Chipset-reference)

* WS281x Clockless family
  * WS2811 (Old style 400khz & 800khz)
  * WS2812 (NeoPixel)
  * WS2812-V5B (250 uS reset)
  * WS2815
* APA102 / SK9822 / HD107s (turbo->40mhz) / Adafruit DotStars (SPI)
* HD107s, same thing as the APA102, but runs at turbo 40 Mhz
* SmartMatrix panels - needs the SmartMatrix library (https://github.com/pixelmatix/SmartMatrix)
* TM1809/4 - 3 wire chipset, cheaply available on aliexpress.com
* TM1803 - 3 wire chipset, sold by RadioShack
* UCS1903 - another 3-wire LED chipset, cheap
* GW6205 - another 3-wire LED chipset
* LPD8806 - SPI-based chipset, very high speed
* WS2801 - SPI-based chipset, cheap and widely available
* SM16716 - SPI-based chipset
* APA102 - SPI-based chipset
  * APA102HD - Same as APA102 but with a high-definition gamma correction function applied at the driver level.
* P9813 - aka Cool Neon's Total Control Lighting
* DMX - send rgb data out over DMX using Arduino DMX libraries
* LPD6803 - SPI-based chipset, chip CMODE pin must be set to 1 (inside oscillator mode)

## APA102 and the 'High Definition' Mode in FastLED

FastLED features driver-level gamma correction for the APA102 and SK9822 chipsets, using our "pseudo-13-bit mixing" algorithm.

Read about it here: https://github.com/FastLED/FastLED/blob/master/APA102.md

Enable it like by using the `APA102HD` type. Example:

```C++
#define LED_TYPE APA102HD  // "HD" suffix for APA102 family enables hardware gamma correction
void setup() {
  FastLED.addLeds<LED_TYPE, DATA_PIN, CLOCK_PIN, RGB>(leds_hd, NUM_LEDS);
}
```

![image](https://github.com/user-attachments/assets/999e68ce-454f-4f15-9590-a8d2e8d47a22)

Check out thr rust port of this algorithm:

https://github.com/smart-leds-rs/apa102-spi-rs/pull/15

# Getting Started

### Arduino IDE / PlatformIO Dual Repo

We've created a custom repo you can try to start your projects. This repo is designed to be used with VSCode + PlatformIO but is also *backwards compatible with the Arduino IDE*.

PlatformIO is an extension to VSCode and is generally viewed as a much better experience than the Arduino IDE. You get auto completion tools like intellisense and CoPilot and the ability to install tools like crash decoding. Anything you can do in Arduino IDE you can do with PlatformIO.

Get started here:

https://github.com/FastLED/PlatformIO-Starter

### ArduinoIDE

When running the Arduino IDE you need to do the additional installation step of installing FastLED in the global Arduino IDE package manager.

Install the library using either [the .zip file from the latest release](https://github.com/FastLED/FastLED/releases/latest/) or by searching for "FastLED" in the libraries manager of the Arduino IDE. [See the Arduino documentation on how to install libraries for more information.](https://docs.arduino.cc/software/ide-v1/tutorials/installing-libraries)


## Porting FastLED to a new platform

Information on porting FastLED can be found in the file [PORTING.md](PORTING.md).

## What about that name?

Wait, what happened to FastSPI_LED and FastSPI_LED2?  The library was initially named FastSPI_LED because it was focused on very fast and efficient SPI access.  However, since then, the library has expanded to support a number of LED chipsets that don't use SPI, as well as a number of math and utility functions for LED processing across the board.  We decided that the name FastLED more accurately represents the totality of what the library provides, everything fast, for LEDs.

## For more information

Check out the official site http://fastled.io for links to documentation, issues, and news.

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

## Contributing

See our easy to use guide here:

https://github.com/FastLED/FastLED/blob/master/CONTRIBUTING.md

*To stay updated on the latest feature releases, please click the `Watch` button in the upper right*
