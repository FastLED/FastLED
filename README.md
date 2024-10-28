FastLED
===========
[![arduino-library-badge](https://www.ardu-badge.com/badge/FastLED.svg)](https://www.ardu-badge.com/FastLED)
[![build status](https://github.com/FastLED/FastLED/workflows/build/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build.yml)
[![unit tests](https://github.com/FastLED/FastLED/actions/workflows/build_unit_test.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_unit_test.yml)
[![Arduino Library Lint](https://github.com/FastLED/FastLED/actions/workflows/arduino_library_lint.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/arduino_library_lint.yml)
[![Documentation](https://img.shields.io/badge/Docs-Doxygen-blue.svg)](http://fastled.io/docs)
[![Reddit](https://img.shields.io/badge/reddit-/r/FastLED-orange.svg?logo=reddit)](https://www.reddit.com/r/FastLED/)




## About

The led driver for tiny computers the size of a quarter, more or less.

esp32, teensy, arduino,
raspberri pi, attiny family and more.

This is a library for easily & efficiently controlling a wide variety of LED chipsets, like the ones
sold by Adafruit (NeoPixel, DotStar, LPD8806), Sparkfun (WS2801), and AliExpress.  In addition to writing to the
LEDs, this library also includes a number of functions for high-performing 8-bit math for manipulating
your RGB values, as well as low level classes for abstracting out access to pins and SPI hardware, while
still keeping things as fast as possible.

We have multiple goals with this library:

* Quick start for new developers - hook up your LEDs and go, no need to think about specifics of the LED chipsets being used
* Zero pain switching LED chipsets - you get some new LEDs that the library supports, just change the definition of LEDs you're using, et. voila!  Your code is running with the new LEDs.
* High performance - with features like zero cost global brightness scaling, high performance 8-bit math for RGB manipulation, and some of the fastest bit-bang'd SPI support around, FastLED wants to keep as many CPU cycles available for your LED patterns as possible

## Example

*This is an Arduino Sketch that will run on Arduino Uno/Esp32/Raspberri Pi*
```C++
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

For more examples see this [link](examples).

## Supported Platforms
#### Arduino

[![uno](https://github.com/FastLED/FastLED/actions/workflows/build_uno.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_uno.yml)


[![attiny13](https://github.com/FastLED/FastLED/actions/workflows/build_attiny13.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_attiny13.yml)
*needs pin definitions for this board*


[![attiny85](https://github.com/FastLED/FastLED/actions/workflows/build_attiny85.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_attiny85.yml)


[![attiny1604](https://github.com/FastLED/FastLED/actions/workflows/build_attiny1604.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_attiny1604.yml)


[![attiny1616](https://github.com/FastLED/FastLED/actions/workflows/build_attiny1616.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_attiny1616.yml)


[![attiny4313](https://github.com/FastLED/FastLED/actions/workflows/build_attiny4313.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_attiny4313.yml)
*needs pin definitions for this board*


[![yun](https://github.com/FastLED/FastLED/actions/workflows/build_yun.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_yun.yml)


[![digix](https://github.com/FastLED/FastLED/actions/workflows/build_digix.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_digix.yml)


[![uno_r4_wifi](https://github.com/FastLED/FastLED/actions/workflows/build_uno_r4_wifif.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_uno_r4_wifif.yml)


[![nano_every](https://github.com/FastLED/FastLED/actions/workflows/build_nano_every.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_nano_every.yml)


#### Teensy
[![teensy30](https://github.com/FastLED/FastLED/actions/workflows/build_teensy30.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_teensy30.yml)


[![teensy31](https://github.com/FastLED/FastLED/actions/workflows/build_teensy31.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_teensy31.yml)


[![teensyLC](https://github.com/FastLED/FastLED/actions/workflows/build_teensyLC.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_teensyLC.yml)


[![teensy40](https://github.com/FastLED/FastLED/actions/workflows/build_teensy40.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_teensy40.yml)


[![teensy41](https://github.com/FastLED/FastLED/actions/workflows/build_teensy41.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_teensy41.yml)

*Specific Features*

[![teensy_octoWS2811](https://github.com/FastLED/FastLED/actions/workflows/build_teensy_octo.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_teensy_octo.yml)

#### NRF

[![nrf52840_sense](https://github.com/FastLED/FastLED/actions/workflows/build_adafruit_feather_nrf52840_sense.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_adafruit_feather_nrf52840_sense.yml)

[![nordicnrf52_dk](https://github.com/FastLED/FastLED/actions/workflows/build_nrf52840_dk.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_nrf52840_dk.yml)
(new board, for some reason it's missing nrf.h, nrf_spim.h, nrf_pwm.h and nrf_nvic.h on our platformio based build system, might work in arduino.h)

[![adafruit_xiaoblesense](https://github.com/FastLED/FastLED/actions/workflows/build_adafruit_xiaoblesense.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_adafruit_xiaoblesense.yml)

#### STM

[![bluepill](https://github.com/FastLED/FastLED/actions/workflows/build_bluepill.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_bluepill.yml)

[![maple_mini](https://github.com/FastLED/FastLED/actions/workflows/build_maple_map.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_maple_map.yml)

[![stm103tb](https://github.com/FastLED/FastLED/actions/workflows/build_stm103tb.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_stm103tb.yml)
(PlatformIO doesn't support this board yet and we don't know what the build info is to support this is yet)

#### Raspberry Pi

[![rp2040](https://github.com/FastLED/FastLED/actions/workflows/build_rp2040.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_rp2040.yml)


[![rp2350](https://github.com/FastLED/FastLED/actions/workflows/build_rp2350.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_rp2350.yml)


#### Esp

[![esp32-8266](https://github.com/FastLED/FastLED/actions/workflows/build_esp8622.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_esp8622.yml)


[![esp32dev](https://github.com/FastLED/FastLED/actions/workflows/build_esp32dev.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_esp32dev.yml)


[![esp32wroom](https://github.com/FastLED/FastLED/actions/workflows/build_esp32wroom.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_esp32wroom.yml)


[![esp32c2](https://github.com/FastLED/FastLED/actions/workflows/build_esp32c2.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_esp32c2.yml)
*might work with alternative settings, missing RMT device*


[![esp32c3](https://github.com/FastLED/FastLED/actions/workflows/build_esp32c3.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_esp32c3.yml)


[![esp32s3](https://github.com/FastLED/FastLED/actions/workflows/build_esp32s3.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_esp32s3.yml)


[![esp32c6](https://github.com/FastLED/FastLED/actions/workflows/build_esp32c6.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_esp32c6.yml)


[![esp32h2](https://github.com/FastLED/FastLED/actions/workflows/build_esp32h2.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_esp32h2.yml)

*Specific features*

[![esp32rmt_51](https://github.com/FastLED/FastLED/actions/workflows/build_esp32rmt.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_esp32rmt.yml)


[![esp32_i2s_ws2812](https://github.com/FastLED/FastLED/actions/workflows/build_esp32_i2s_ws2812.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_esp32_i2s_ws2812.yml)

Espressif's current evaluation of FastLED's compatibility with their product sheet can be found [here](https://github.com/espressif/arduino-esp32/blob/gh-pages/LIBRARIES_TEST.md)


### x86

[![linux_native](https://github.com/FastLED/FastLED/actions/workflows/build_linux.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_linux.yml)

### Wasm

[![wasm](https://github.com/FastLED/FastLED/actions/workflows/build_wasm.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_wasm.yml)


## Compiled Library Size Check

[![attiny85_binary_size](https://github.com/FastLED/FastLED/actions/workflows/check_attiny85.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/check_attiny85.yml)

[![uno_binary_size](https://github.com/FastLED/FastLED/actions/workflows/check_uno_size.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/check_uno_size.yml)


[![esp32dev_binary_size](https://github.com/FastLED/FastLED/actions/workflows/check_esp32_size.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/check_esp32_size.yml)


[![teensy41_binary_size](https://github.com/FastLED/FastLED/actions/workflows/check_teensy41_size.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/check_teensy41_size.yml)


## Getting Started

### Arduino IDE / PlatformIO Dual Repo

We've created a custom repo you can try to start your projects. This repo is designed to be used with VSCode + PlatformIO but is also *backwards compatible with the Arduino IDE*.

PlatformIO is an extension to VSCode and is generally viewed as a much better experience than the Arduino IDE. You get auto completion tools like intellisense and CoPilot and the ability to install tools like crash decoding. Anything you can do in Arduino IDE you can do with PlatformIO.

Get started here:

https://github.com/FastLED/PlatformIO-Starter

### ArduinoIDE

When running the Arduino IDE you need to do the additional installation step of installing FastLED in the global Arduino IDE package manager.

Install the library using either [the .zip file from the latest release](https://github.com/FastLED/FastLED/releases/latest/) or by searching for "FastLED" in the libraries manager of the Arduino IDE. [See the Arduino documentation on how to install libraries for more information.](https://docs.arduino.cc/software/ide-v1/tutorials/installing-libraries)

## Development

[![clone and compile](https://github.com/FastLED/FastLED/actions/workflows/build_default.yml/badge.svg)](https://github.com/FastLED/FastLED/actions/workflows/build_default.yml)

If you want to make changes to FastLED then please

  * [Fork](https://github.com/FastLED/FastLED/fork) the https://github.com/FastLED/FastLED repo into your github account.
  * Open up the folder with VSCode.
    * Make sure VSCode had the platformio extension.
  * Once FastLED is loading with platformio, give it some time to download the dependencies (esp32-s3 (default) has a 1+GB download!)
  * Click the platformio compile
    * Then upload to your device
  * See [dev/dev.ino](dev/dev.ino).
<img width="1220" alt="image" src="https://github.com/user-attachments/assets/66f1832d-3cfb-4633-8af8-e66148bcad1b">

When changes are made then push to your fork to your repo and git will give you a url to trigger a pull request into the master repo.

### Testing other devices

  * run [compile](compile) and then select your board

```bash
Available boards:
[0]: uno
[1]: esp32dev
[2]: esp01
[3]: esp32-c3-devkitm-1
[4]: esp32-s3-devkitc-1
[5]: yun
[6]: digix
[7]: teensy30
[8]: teensy41
Enter the number of the board you want to use: 0
```

## Help and Support

If you need help with using the library, please consider visiting the Reddit community at https://reddit.com/r/FastLED. There are thousands of knowledgeable FastLED users in that group and a plethora of solutions in the post history.

If you are looking for documentation on how something in the library works, please see the Doxygen documentation online at http://fastled.io/docs.

If you run into bugs with the library, or if you'd like to request support for a particular platform or LED chipset, please submit an issue at http://fastled.io/issues.

## Supported LED Chipsets

Here's a list of all the LED chipsets are supported.  More details on the LED chipsets are included [on our wiki page](https://github.com/FastLED/FastLED/wiki/Chipset-reference)

* Adafruit's DotStars - aka APA102
* Adafruit's Neopixel - aka WS2812B (also WS2811/WS2812/WS2813, also supported in lo-speed mode) - a 3 wire addressable LED chipset
* TM1809/4 - 3 wire chipset, cheaply available on aliexpress.com
* TM1803 - 3 wire chipset, sold by RadioShack
* UCS1903 - another 3 wire LED chipset, cheap
* GW6205 - another 3 wire LED chipset
* LPD8806 - SPI based chipset, very high speed
* WS2801 - SPI based chipset, cheap and widely available
* SM16716 - SPI based chipset
* APA102 - SPI based chipset
  * APA102HD - Same as APA102 but with a high definition gamma correction function applied at the driver level.
* P9813 - aka Cool Neon's Total Control Lighting
* DMX - send rgb data out over DMX using Arduino DMX libraries
* SmartMatrix panels - needs the SmartMatrix library (https://github.com/pixelmatix/SmartMatrix)
* LPD6803 - SPI based chpiset, chip CMODE pin must be set to 1 (inside oscillator mode)

HL1606, and "595"-style shift registers are no longer supported by the library.  The older Version 1 of the library ("FastSPI_LED") has support for these, but is missing many of the advanced features of current versions and is no longer being maintained.

## Supported Platforms

Right now the library is supported on a variety of arduino compatible platforms.  If it's ARM or AVR and uses the arduino software (or a modified version of it to build) then it is likely supported.  Note that we have a long list of upcoming platforms to support, so if you don't see what you're looking for here, ask, it may be on the roadmap (or may already be supported).  N.B. at the moment we are only supporting the stock compilers that ship with the arduino software.  Support for upgraded compilers, as well as using AVR studio and skipping the arduino entirely, should be coming in a near future release.

* Adafruit Trinket & Gemma - Trinket Pro may be supported, but haven't tested to confirm yet
* Arduino & compatibles - straight up Arduino devices, Uno, Duo, Leonardo, Mega, Nano, etc...
* Arduino Due and the digistump DigiX
* Arduino Yún
* Arduino Zero
* AVR microcontrollers - ATtiny, ATmega and more families
* ESP32 based boards
* ESP8266 using the Arduino board definitions from http://arduino.esp8266.com/stable/package_esp8266com_index.json - please be sure to also read https://github.com/FastLED/FastLED/wiki/ESP8266-notes for information specific to the 8266.
* Teensy 2, Teensy++ 2, Teensy 3.0, Teensy 3.1/3.2, Teensy LC, Teensy 3.5, Teensy 3.6, and Teensy 4.0 - arduino compatible from pjrc.com with some extra goodies (note the teensy LC, 3.2, 3.5, 3.6, 4.0 are ARM, not AVR!)
* RFDuino
* SparkCore
* The wino board - http://wino-board.com

What types of platforms are we thinking about supporting in the future?  Here's a short list:  ChipKit32, Maple, Beagleboard

### Porting FastLED to a new platform

Information on porting FastLED can be found in the file
[PORTING.md](./PORTING.md).

## What about that name?

Wait, what happened to FastSPI_LED and FastSPI_LED2?  The library was initially named FastSPI_LED because it was focused on very fast and efficient SPI access.  However, since then, the library has expanded to support a number of LED chipsets that don't use SPI, as well as a number of math and utility functions for LED processing across the board.  We decided that the name FastLED more accurately represents the totality of what the library provides, everything fast, for LEDs.

## For more information

Check out the official site http://fastled.io for links to documentation, issues, and news


*TODO* - get candy
